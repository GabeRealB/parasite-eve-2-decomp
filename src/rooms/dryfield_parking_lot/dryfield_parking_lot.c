#include "common.h"
#include "rooms/dryfield_parking_lot.h"
#include "rooms/dryfield_night_parking_lot.h"
#include "rooms/room.h"
#include "rooms/room_common.h"

#include "gameplay/captions.h"
#include "gameplay/area_transitions.h"
#include "gameplay/display.h"
#include "gameplay/items.h"
#include "gameplay/message.h"
#include "gameplay/room_effects.h"
#include "gameplay/loading.h"
#include "gameplay/world_targets.h"

#include "gameplay/scene.h"
#include "gameplay/world_state.h"
#include "main/display.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"

#include "actors/task_tables.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/direction_input.h"
#include "gameplay/light.h"
#include "gameplay/room.h"
#include "gameplay/view.h"
#include "mapui/stage_tables.h"
#include "rooms/stage_tables.h"

// Preserve the following nonzero bytes with this scalar's storage.
// No separate references identify them; their role (including padding) is unresolved.
extern u8 D_dryfield_parking_lot_8017FB58[4];

/// The `GpAreaApplyRec` list the 0x11 answer applies when the event fires.
/// The address sits past the end of this package, so the linker resolves it
/// from its auto-generated undefined-symbols file as an absolute one rather
/// than as room data.

/// Descriptor of the event task the event gate spawns.
extern TaskDesc D_dryfield_parking_lot_8017DBF8;

/// The room's message table, published in `Task::msgTable` by the entry task
/// (ids 0x13EE-0x13F2).
extern GpMsgEntry D_dryfield_parking_lot_8017DC04[];

/// Per-view values `func_dryfield_parking_lot_8017DBAC` publishes, indexed by
/// camera view index minus one.
extern u16 D_dryfield_parking_lot_8017DC34[];

/// The message and request the event gate latched for its event task.
extern RoomEventMsg D_dryfield_parking_lot_8017FB50;
extern RoomEventReq D_dryfield_parking_lot_8017FB5C;

/// Set by the event gate when its last call latched a request and spawned the
/// event task; every call clears it first.

extern GpGridParams D_dryfield_parking_lot_8017E8DC[1];
extern GpObj3A D_dryfield_parking_lot_8017F6E4[2];
extern GpObj4C D_dryfield_parking_lot_8017F0A8[10];
extern GpObj4C D_dryfield_parking_lot_8017F3A0[11];
extern GpRoomCoordSet D_dryfield_parking_lot_8017F9FC[1];
extern TaskDesc D_8014D8A4;
s32 func_dryfield_parking_lot_8017D8BC(Task *, s32, RoomEventMsg *, RoomEventMsg *);
s32 func_dryfield_parking_lot_8017DAA0(Task *, s32, s32, GpMessageArg);
s32 func_dryfield_parking_lot_8017DAF0(Task *, s32, GpMessageArg, GpMessageArg);
s32 func_dryfield_parking_lot_8017DAF8(Task *, s32, GpMessageArg, GpMessageArg);
s32 func_dryfield_parking_lot_8017DB00(Task *, s32, GpMessageArg, GpMessageArg);
void func_dryfield_parking_lot_8017D74C(Task *);

TaskDesc D_dryfield_parking_lot_8017DBF8 = { 0, 32, func_dryfield_parking_lot_8017D74C, { .model = NULL } };

GpMsgEntry D_dryfield_parking_lot_8017DC04[6] = {
    { 5102, func_dryfield_parking_lot_8017D8BC },
    { 5105, func_dryfield_parking_lot_8017DAF0 },
    { 5103, func_dryfield_parking_lot_8017DB00 },
    { 5104, func_dryfield_parking_lot_8017DAF8 },
    { 5106, func_dryfield_parking_lot_8017DAA0 },
    { 0x7FFFFFFF, NULL },
};

u16 D_dryfield_parking_lot_8017DC34[8] = {
    0,
    2,
    2,
    2,
    2,
    0,
    0,
    0xE5E6,
};

GpRoomObjRec D_dryfield_parking_lot_8017DC44[1] = {
    { D_dryfield_parking_lot_8017E8DC, D_dryfield_parking_lot_8017F0A8, D_dryfield_parking_lot_8017F3A0, D_dryfield_parking_lot_8017F6E4 },
};

u8 * D_dryfield_parking_lot_8017DC54[1] = {
    D_8010CAF8,
};

GpViewCountRec D_dryfield_parking_lot_8017DC58[1] = {
    { { .bytes = { 7, 0 } } },
};

GpRoomCoordRec D_dryfield_parking_lot_8017DC5C[1] = {
    { D_dryfield_parking_lot_8017F9FC, NULL },
};

GpWarpRec D_dryfield_parking_lot_8017DC64[5] = {
    { { .words = { 3072, 0x2995, 0, 3077 } }, { 0, 0, 0, 0 }, { .words = { 3072, 0x2995, 0, 3077 } }, { 0, 0, 0, 0 }, 0x520F0002, 0x520F0001, 0, 7, 0, 486 },
    { { .words = { 2048, 3200, 0, 3472 } }, { 0, 0, 0, 0 }, { .words = { 2048, 3200, 0, 3472 } }, { 0, 0, 0, 0 }, 0x520F0008, 0x520F0007, 0x520F000A, 5, 0, 481 },
    { { .words = { 0, -285, 0, 351 } }, { 0, 0, 0, 0 }, { .words = { 0, -285, 0, 351 } }, { 0, 0, 0, 0 }, 0x520F0006, 0x520F0005, 0x520F000A, 4, 0, 480 },
    { { .words = { 2048, -6840, 0, -450 } }, { 0, 0, 0, 0 }, { .words = { 2048, -6840, 0, -450 } }, { 0, 0, 0, 0 }, 0x520F0008, 0x520F0007, 0, 2, 0, 479 },
    { { .words = { 0, 4771, -450, 1414 } }, { 0, 0, 0, 0 }, { .words = { 0, 4491, 0, 2215 } }, { 0, 0, 0, 0 }, 0, 0, 0, 6, 1, 0 },
};

SVECTOR D_dryfield_parking_lot_8017DD7C[18] = {
    { 0, -4096, 0, 0 },
    { 4093, 0, 152, 0 },
    { 272, 0, 4087, 0 },
    { -4096, 0, 0, 0 },
    { -946, 0, 3985, 0 },
    { 3041, 0, -2744, 0 },
    { 4096, 0, 0, 0 },
    { 0, 0, -4096, 0 },
    { 2994, 0, -2795, 0 },
    { -2914, 0, -2879, 0 },
    { 0, 0, 4096, 0 },
    { 3247, 0, -2497, 0 },
    { 3147, 0, 2622, 0 },
    { 0, -3417, 2259, 0 },
    { 0, -3437, -2228, 0 },
    { -1959, 0, 3597, 0 },
    { 4006, 0, 855, 0 },
    { 0, 4096, 0, 0 },
};

SVECTOR D_dryfield_parking_lot_8017DE0C[132] = {
    { -0x2BC0, -4606, -3600, 0 },
    { -8400, -4606, 200, 0 },
    { -8300, -4606, -2500, 0 },
    { -6800, -4606, -2600, 0 },
    { -8400, 0, 200, 0 },
    { -8300, 0, -2500, 0 },
    { -6800, 0, -2600, 0 },
    { 5200, 310, 1715, 0 },
    { 5200, -3400, 1715, 0 },
    { 5200, -3400, -250, 0 },
    { 5200, 310, -250, 0 },
    { 1200, 0, 2000, 0 },
    { 0x35E8, 0, 2000, 0 },
    { 0x35E8, 0, -8900, 0 },
    { 1200, 0, -8900, 0 },
    { 6400, 310, 2000, 0 },
    { 6400, -3400, 2000, 0 },
    { -3300, -1520, 2900, 0 },
    { -4500, -1520, 4400, 0 },
    { 0, -1520, 4400, 0 },
    { -1400, -1520, 2900, 0 },
    { -4500, 640, -30, 0 },
    { -4500, -1520, -30, 0 },
    { -3300, -1520, 1300, 0 },
    { -3300, 640, 1300, 0 },
    { -3300, 640, 2900, 0 },
    { -1400, 640, 2900, 0 },
    { 0, 640, 4400, 0 },
    { -4996, -4000, -335, 0 },
    { -4996, 0, -335, 0 },
    { -5327, 0, 0, 0 },
    { -5327, -4000, 0, 0 },
    { 5890, -1498, -237, 0 },
    { 5890, -1798, -237, 0 },
    { 5890, -1798, -1367, 0 },
    { 5890, -1498, -1367, 0 },
    { 4190, -1798, -1367, 0 },
    { 4190, -1498, -1367, 0 },
    { 4190, -1498, -237, 0 },
    { 5090, -1498, -363, 0 },
    { 5090, -1798, -363, 0 },
    { 5190, -1798, -233, 0 },
    { 5190, -1498, -233, 0 },
    { 4190, -1498, -363, 0 },
    { 4190, -1798, -363, 0 },
    { 5090, -3398, 2037, 0 },
    { 5090, -3098, 2037, 0 },
    { 5190, -3098, 1917, 0 },
    { 5190, -3398, 1917, 0 },
    { 4190, -3398, 2037, 0 },
    { 4190, -3098, 2037, 0 },
    { 4190, -3398, 4037, 0 },
    { 4190, -3098, 4037, 0 },
    { 4190, -3098, 1917, 0 },
    { 4190, -3398, 1917, 0 },
    { 6190, -3098, 1917, 0 },
    { 6190, -3098, 4037, 0 },
    { 6190, -3398, 4037, 0 },
    { 4190, 0, 1715, 0 },
    { 5190, 0, 1715, 0 },
    { 5190, -200, 1715, 0 },
    { 4190, -200, 1715, 0 },
    { 4190, -1496, -245, 0 },
    { 5190, -1496, -245, 0 },
    { 5190, -1696, -245, 0 },
    { 4190, -1696, -245, 0 },
    { 1200, 0, 6500, 0 },
    { 0x35E8, 0, 6500, 0 },
    { 0x2AF8, 500, 5000, 0 },
    { 0x2AF8, -3500, 5000, 0 },
    { 0x2AF8, -3500, 1000, 0 },
    { 0x2AF8, 500, 1000, 0 },
    { -1928, 0, 319, 0 },
    { -1928, -2500, 319, 0 },
    { -1928, -2500, -5000, 0 },
    { -1928, 0, -5000, 0 },
    { 4200, -2500, 319, 0 },
    { 4200, -2500, -5000, 0 },
    { 4200, 0, -5000, 0 },
    { 4200, 0, 319, 0 },
    { -0x3FAC, -5041, -4680, 0 },
    { -0x3FAC, 0, -4680, 0 },
    { 0x396C, 0, -4680, 0 },
    { 0x396C, -5041, -4680, 0 },
    { 5190, -3093, 1918, 0 },
    { 6190, -3093, 1918, 0 },
    { 6190, -1693, -242, 0 },
    { 5190, -1693, -242, 0 },
    { 5190, -3293, 1918, 0 },
    { 6190, -3293, 1918, 0 },
    { 6190, -1493, -242, 0 },
    { 5190, -1493, -242, 0 },
    { -6324, 0, -2061, 0 },
    { -6324, -4236, -2061, 0 },
    { -0x3084, -4236, -5381, 0 },
    { -0x3084, 0, -5381, 0 },
    { -5750, -4236, -4749, 0 },
    { -8846, -4236, -6469, 0 },
    { -5750, 0, -4749, 0 },
    { -0x36B0, 0, 0, 0 },
    { -0x36B0, -4000, 0, 0 },
    { 0x2A94, -3000, 3800, 0 },
    { 0x2A94, 0, 3800, 0 },
    { 5900, 0, 3800, 0 },
    { 5900, -3000, 3800, 0 },
    { -4000, -4000, 3800, 0 },
    { -4000, 0, 3800, 0 },
    { -4000, 0, -335, 0 },
    { -4000, -4000, -335, 0 },
    { 6800, -4000, 3800, 0 },
    { 6800, 0, 3800, 0 },
    { 5900, -3000, -4900, 0 },
    { 5900, -3000, 2180, 0 },
    { 5900, -6000, 2000, 0 },
    { 5900, -6000, -4900, 0 },
    { 5900, 0, -4900, 0 },
    { 5900, 0, 2180, 0 },
    { 5900, -6000, 3800, 0 },
    { 5900, -3000, 0x2AF8, 0 },
    { 5900, -6000, 0x2AF8, 0 },
    { 5900, 0, 0x2AF8, 0 },
    { 0x2A94, -3000, 2180, 0 },
    { 0x2A94, 0, 2180, 0 },
    { -0x4588, 0, 2000, 0 },
    { -0x4588, 0, -8900, 0 },
    { -0x4588, 0, 6500, 0 },
    { 4190, 0, 1715, 0 },
    { 4190, -3000, 1715, 0 },
    { 4190, -3000, -4680, 0 },
    { 4190, 0, -4680, 0 },
    { 6190, 0, 1715, 0 },
    { 6190, -3000, 1715, 0 },
};

GpGridFace D_dryfield_parking_lot_8017E22C[55] = {
    { { 1, 2, 0, 3 }, 0, 0 },
    { { 4, 5, 1, 2 }, 1, 0 },
    { { 5, 6, 2, 3 }, 2, 0 },
    { { 8, 9, 7, 10 }, 3, 0 },
    { { 12, 13, 11, 14 }, 0, 5 },
    { { 16, 8, 15, 7 }, 4, 0 },
    { { 18, 19, 17, 20 }, 0, 0 },
    { { 22, 23, 21, 24 }, 5, 0 },
    { { 23, 17, 24, 25 }, 6, 0 },
    { { 17, 20, 25, 26 }, 7, 0 },
    { { 20, 19, 26, 27 }, 8, 0 },
    { { 29, 30, 28, 31 }, 9, 0 },
    { { 33, 34, 32, 35 }, 3, 0 },
    { { 34, 36, 35, 37 }, 10, 0 },
    { { 38, 32, 37, 35 }, 0, 0 },
    { { 40, 41, 39, 42 }, 11, 0 },
    { { 39, 43, 40, 44 }, 7, 0 },
    { { 46, 47, 45, 48 }, 12, 0 },
    { { 45, 49, 46, 50 }, 10, 0 },
    { { 52, 53, 51, 54 }, 6, 0 },
    { { 53, 52, 55, 56 }, 0, 0 },
    { { 56, 52, 57, 51 }, 7, 0 },
    { { 59, 60, 58, 61 }, 10, 0 },
    { { 63, 64, 62, 65 }, 10, 0 },
    { { 62, 61, 63, 60 }, 13, 0 },
    { { 11, 66, 12, 67 }, 0, 2 },
    { { 69, 70, 68, 71 }, 3, 0 },
    { { 73, 74, 72, 75 }, 3, 0 },
    { { 76, 77, 73, 74 }, 0, 0 },
    { { 77, 76, 78, 79 }, 6, 0 },
    { { 76, 73, 79, 72 }, 10, 0 },
    { { 81, 82, 80, 83 }, 10, 1 },
    { { 85, 86, 84, 87 }, 14, 0 },
    { { 84, 88, 85, 89 }, 7, 0 },
    { { 91, 87, 90, 86 }, 7, 0 },
    { { 93, 94, 92, 95 }, 15, 0 },
    { { 96, 97, 93, 94 }, 0, 0 },
    { { 96, 93, 98, 92 }, 16, 0 },
    { { 30, 99, 31, 100 }, 7, 0 },
    { { 102, 103, 101, 104 }, 7, 0 },
    { { 106, 107, 105, 108 }, 6, 0 },
    { { 28, 108, 29, 107 }, 7, 0 },
    { { 109, 110, 105, 106 }, 7, 0 },
    { { 112, 113, 111, 114 }, 3, 0 },
    { { 111, 115, 112, 116 }, 3, 0 },
    { { 104, 118, 117, 119 }, 3, 0 },
    { { 117, 113, 104, 112 }, 3, 0 },
    { { 103, 120, 104, 118 }, 3, 0 },
    { { 104, 112, 101, 121 }, 17, 0 },
    { { 112, 116, 121, 122 }, 10, 0 },
    { { 17, 23, 18, 22 }, 0, 0 },
    { { 11, 14, 123, 124 }, 0, 5 },
    { { 123, 125, 11, 66 }, 0, 5 },
    { { 127, 128, 126, 129 }, 3, 6 },
    { { 131, 127, 130, 126 }, 10, 6 },
};

s16 D_dryfield_parking_lot_8017E4C0[5] = {
    31,
    35,
    36,
    51,
    -1,
};

s16 D_dryfield_parking_lot_8017E4CC[4] = {
    31,
    38,
    51,
    -1,
};

s16 D_dryfield_parking_lot_8017E4D4[4] = {
    38,
    51,
    52,
    -1,
};

s16 D_dryfield_parking_lot_8017E4DC[3] = {
    51,
    52,
    -1,
};

s16 D_dryfield_parking_lot_8017E4E4[2] = {
    52,
    -1,
};

s16 D_dryfield_parking_lot_8017E4E8[6] = {
    0,
    31,
    35,
    36,
    51,
    -1,
};

s16 D_dryfield_parking_lot_8017E4F4[9] = {
    0,
    1,
    2,
    31,
    35,
    36,
    38,
    51,
    -1,
};

s16 D_dryfield_parking_lot_8017E508[6] = {
    0,
    1,
    38,
    51,
    52,
    -1,
};

s16 D_dryfield_parking_lot_8017E514[3] = {
    51,
    52,
    -1,
};

s16 D_dryfield_parking_lot_8017E51C[2] = {
    52,
    -1,
};

s16 D_dryfield_parking_lot_8017E520[7] = {
    0,
    31,
    35,
    36,
    37,
    51,
    -1,
};

s16 D_dryfield_parking_lot_8017E530[12] = {
    0,
    1,
    2,
    11,
    31,
    35,
    36,
    37,
    38,
    41,
    51,
    -1,
};

s16 D_dryfield_parking_lot_8017E548[15] = {
    0,
    1,
    2,
    7,
    11,
    35,
    36,
    37,
    38,
    40,
    41,
    50,
    51,
    52,
    -1,
};

s16 D_dryfield_parking_lot_8017E568[7] = {
    6,
    40,
    42,
    50,
    51,
    52,
    -1,
};

s16 D_dryfield_parking_lot_8017E578[2] = {
    52,
    -1,
};

s16 D_dryfield_parking_lot_8017E57C[7] = {
    27,
    28,
    31,
    36,
    37,
    51,
    -1,
};

s16 D_dryfield_parking_lot_8017E58C[17] = {
    0,
    2,
    7,
    11,
    27,
    28,
    30,
    31,
    35,
    36,
    37,
    38,
    40,
    41,
    50,
    51,
    -1,
};

s16 D_dryfield_parking_lot_8017E5B0[20] = {
    6,
    7,
    8,
    9,
    10,
    11,
    27,
    28,
    30,
    35,
    36,
    37,
    38,
    40,
    41,
    42,
    50,
    51,
    52,
    -1,
};

s16 D_dryfield_parking_lot_8017E5D8[11] = {
    6,
    7,
    8,
    9,
    10,
    40,
    42,
    50,
    51,
    52,
    -1,
};

s16 D_dryfield_parking_lot_8017E5F0[2] = {
    52,
    -1,
};

s16 D_dryfield_parking_lot_8017E5F4[6] = {
    4,
    27,
    28,
    31,
    51,
    -1,
};

s16 D_dryfield_parking_lot_8017E600[9] = {
    4,
    27,
    28,
    29,
    30,
    31,
    51,
    53,
    -1,
};

s16 D_dryfield_parking_lot_8017E614[23] = {
    4,
    6,
    7,
    8,
    9,
    10,
    18,
    19,
    20,
    22,
    24,
    25,
    27,
    28,
    29,
    30,
    42,
    50,
    51,
    52,
    53,
    54,
    -1,
};

s16 D_dryfield_parking_lot_8017E644[9] = {
    4,
    6,
    9,
    10,
    25,
    42,
    51,
    52,
    -1,
};

s16 D_dryfield_parking_lot_8017E658[3] = {
    25,
    52,
    -1,
};

s16 D_dryfield_parking_lot_8017E660[9] = {
    4,
    28,
    29,
    31,
    43,
    44,
    51,
    53,
    -1,
};

s16 D_dryfield_parking_lot_8017E674[20] = {
    3,
    4,
    12,
    13,
    14,
    15,
    16,
    23,
    24,
    28,
    29,
    30,
    31,
    32,
    34,
    43,
    44,
    51,
    53,
    -1,
};

s16 D_dryfield_parking_lot_8017E69C[37] = {
    3,
    4,
    5,
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
    22,
    23,
    24,
    25,
    28,
    29,
    30,
    32,
    33,
    34,
    39,
    42,
    43,
    44,
    45,
    46,
    47,
    48,
    49,
    51,
    52,
    53,
    54,
    -1,
};

s16 D_dryfield_parking_lot_8017E6E8[26] = {
    3,
    4,
    5,
    17,
    18,
    19,
    20,
    21,
    22,
    24,
    25,
    32,
    33,
    39,
    42,
    43,
    44,
    45,
    46,
    47,
    48,
    49,
    52,
    53,
    54,
    -1,
};

s16 D_dryfield_parking_lot_8017E71C[5] = {
    25,
    45,
    47,
    52,
    -1,
};

s16 D_dryfield_parking_lot_8017E728[5] = {
    4,
    31,
    43,
    44,
    -1,
};

s16 D_dryfield_parking_lot_8017E734[18] = {
    3,
    4,
    12,
    13,
    14,
    15,
    16,
    23,
    24,
    28,
    29,
    31,
    32,
    34,
    43,
    44,
    53,
    -1,
};

s16 D_dryfield_parking_lot_8017E758[36] = {
    3,
    4,
    5,
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
    22,
    23,
    24,
    25,
    26,
    28,
    29,
    30,
    32,
    33,
    34,
    39,
    42,
    43,
    44,
    45,
    46,
    47,
    48,
    49,
    53,
    54,
    -1,
};

s16 D_dryfield_parking_lot_8017E7A0[19] = {
    4,
    5,
    20,
    21,
    25,
    26,
    32,
    33,
    39,
    42,
    43,
    44,
    45,
    46,
    47,
    48,
    49,
    54,
    -1,
};

s16 D_dryfield_parking_lot_8017E7C8[4] = {
    25,
    45,
    47,
    -1,
};

s16 D_dryfield_parking_lot_8017E7D0[3] = {
    4,
    31,
    -1,
};

s16 D_dryfield_parking_lot_8017E7D8[4] = {
    4,
    26,
    31,
    -1,
};

s16 D_dryfield_parking_lot_8017E7E0[7] = {
    4,
    25,
    26,
    39,
    48,
    49,
    -1,
};

s16 D_dryfield_parking_lot_8017E7F0[7] = {
    4,
    25,
    26,
    39,
    48,
    49,
    -1,
};

s16 D_dryfield_parking_lot_8017E800[2] = {
    25,
    -1,
};

s16 D_dryfield_parking_lot_8017E804[3] = {
    4,
    31,
    -1,
};

s16 D_dryfield_parking_lot_8017E80C[3] = {
    4,
    31,
    -1,
};

s16 D_dryfield_parking_lot_8017E814[3] = {
    4,
    25,
    -1,
};

s16 D_dryfield_parking_lot_8017E81C[3] = {
    4,
    25,
    -1,
};

s16 D_dryfield_parking_lot_8017E824[2] = {
    25,
    -1,
};

s16 * D_dryfield_parking_lot_8017E828[45] = {
    D_dryfield_parking_lot_8017E4C0,
    D_dryfield_parking_lot_8017E4CC,
    D_dryfield_parking_lot_8017E4D4,
    D_dryfield_parking_lot_8017E4DC,
    D_dryfield_parking_lot_8017E4E4,
    D_dryfield_parking_lot_8017E4E8,
    D_dryfield_parking_lot_8017E4F4,
    D_dryfield_parking_lot_8017E508,
    D_dryfield_parking_lot_8017E514,
    D_dryfield_parking_lot_8017E51C,
    D_dryfield_parking_lot_8017E520,
    D_dryfield_parking_lot_8017E530,
    D_dryfield_parking_lot_8017E548,
    D_dryfield_parking_lot_8017E568,
    D_dryfield_parking_lot_8017E578,
    D_dryfield_parking_lot_8017E57C,
    D_dryfield_parking_lot_8017E58C,
    D_dryfield_parking_lot_8017E5B0,
    D_dryfield_parking_lot_8017E5D8,
    D_dryfield_parking_lot_8017E5F0,
    D_dryfield_parking_lot_8017E5F4,
    D_dryfield_parking_lot_8017E600,
    D_dryfield_parking_lot_8017E614,
    D_dryfield_parking_lot_8017E644,
    D_dryfield_parking_lot_8017E658,
    D_dryfield_parking_lot_8017E660,
    D_dryfield_parking_lot_8017E674,
    D_dryfield_parking_lot_8017E69C,
    D_dryfield_parking_lot_8017E6E8,
    D_dryfield_parking_lot_8017E71C,
    D_dryfield_parking_lot_8017E728,
    D_dryfield_parking_lot_8017E734,
    D_dryfield_parking_lot_8017E758,
    D_dryfield_parking_lot_8017E7A0,
    D_dryfield_parking_lot_8017E7C8,
    D_dryfield_parking_lot_8017E7D0,
    D_dryfield_parking_lot_8017E7D8,
    D_dryfield_parking_lot_8017E7E0,
    D_dryfield_parking_lot_8017E7F0,
    D_dryfield_parking_lot_8017E800,
    D_dryfield_parking_lot_8017E804,
    D_dryfield_parking_lot_8017E80C,
    D_dryfield_parking_lot_8017E814,
    D_dryfield_parking_lot_8017E81C,
    D_dryfield_parking_lot_8017E824,
};

GpGridParams D_dryfield_parking_lot_8017E8DC[1] = {
    { NULL, D_dryfield_parking_lot_8017DD7C, D_dryfield_parking_lot_8017DE0C, D_dryfield_parking_lot_8017E22C, D_dryfield_parking_lot_8017E828, 0x4588, 8900, 9, 5, 4000, 55 },
};

GpViewRec D_dryfield_parking_lot_8017E900[7] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { 0, 0x7530, 0 } }, 380 },
    { { { { 2636, 0, -3134 }, { -93, 4094, -78 }, { 3133, 122, 2635 } }, { 0x27F9, 2236, 6567 } }, 230 },
    { { { { 2646, 0, -3126 }, { -3085, 656, -2612 }, { 501, 4043, 424 } }, { 3012, 5975, 537 } }, 230 },
    { { { { 456, 0, 4070 }, { -262, 4087, 29 }, { -4062, -264, 455 } }, { -3566, 1036, -1232 } }, 230 },
    { { { { 1002, 0, -3971 }, { 304, 4083, 76 }, { 3959, -314, 999 } }, { 2133, 1036, -1232 } }, 230 },
    { { { { 371, 0, -4079 }, { 322, 4083, 29 }, { 4066, -323, 370 } }, { -2016, 736, -2302 } }, 230 },
    { { { { 814, 0, -4014 }, { 596, 4050, 121 }, { 3969, -609, 805 } }, { -6266, 636, -2232 } }, 230 },
};

GpSprtCmd D_dryfield_parking_lot_8017E9FC[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_dryfield_parking_lot_8017EA0C[10] = {
    { 143, 0x3FC0, { .fields = { 56, 88 } }, -160, 32, 500, { .fields = { 16, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 80 } }, -104, 40, 500, { .fields = { 64, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 104 } }, -80, 16, 500, { .fields = { 72, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 88 } }, -56, 32, 500, { .fields = { 88, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 96 } }, 0, 24, 500, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 112 } }, 40, 8, 625, { .fields = { 96, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 120 } }, 72, 0, 650, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 80 } }, 0, -32, 2750, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 88 } }, 40, -32, 3000, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 80 } }, 96, -24, 6250, { .fields = { 88, 176 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_dryfield_parking_lot_8017EAD4[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 7, 0, 0, { 1, 0 } },
    { 7, 3, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpDrawAreaRec D_dryfield_parking_lot_8017EAF4[2] = {
    { { 136, 0, 183, 238 }, 2250 },
    { { 0, 0, 0, 0 }, 0xFFFF },
};

GpSprtElem D_dryfield_parking_lot_8017EB08[24] = {
    { 142, 0x3FC0, { .fields = { 120, 8 } }, -128, 72, 625, { .fields = { 32, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 136, 8 } }, -40, 80, 625, { .fields = { 120, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 88, 8 } }, 72, 88, 625, { .fields = { 64, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 0, -80, 625, { .fields = { 16, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, -32, -88, 625, { .fields = { 8, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, -64, -104, 625, { .fields = { 80, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 8, -32, 1350, { .fields = { 40, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 8, -40, 1125, { .fields = { 40, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 24, -40, 925, { .fields = { 16, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, 0, -56, 925, { .fields = { 120, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 16, -80, 925, { .fields = { 8, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 24, -96, 925, { .fields = { 16, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 96 } }, 40, -112, 925, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 40 } }, 104, 16, 925, { .fields = { 72, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 56 } }, -96, -40, 750, { .fields = { 32, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 24 } }, -160, -120, 625, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 32 } }, -112, -120, 625, { .fields = { 32, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 56 } }, 56, -120, 925, { .fields = { 24, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 56 } }, 56, -64, 925, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 56 } }, 112, -64, 925, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 56 } }, 112, -120, 925, { .fields = { 80, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 24 } }, 64, -8, 925, { .fields = { 24, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 24 } }, 112, -8, 925, { .fields = { 24, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -112, 16, 1000, { .fields = { 88, 80 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_dryfield_parking_lot_8017ECE8[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 24, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_dryfield_parking_lot_8017ED00[9] = {
    { 143, 0x3FC0, { .fields = { 56, 48 } }, -160, 72, 550, { .fields = { 0, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 80, 16 } }, 48, -88, 925, { .fields = { 104, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 112, -72, 925, { .fields = { 112, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 96 } }, -80, -32, 1250, { .fields = { 96, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 112 } }, -96, -32, 1250, { .fields = { 56, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 144 } }, -128, -32, 1000, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 152 } }, -160, -32, 1000, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 96 } }, -88, -48, 1975, { .fields = { 80, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 168 } }, -72, -112, 2000, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_dryfield_parking_lot_8017EDB4[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 3, 0, 0, { 1, 0 } },
    { 3, 4, 0, 0, { 2, 0 } },
    { 7, 2, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_dryfield_parking_lot_8017EDDC[7] = {
    { 143, 0x3FC0, { .fields = { 40, 56 } }, 120, 56, 725, { .fields = { 88, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -136, 40, 750, { .fields = { 64, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, -128, 88, 750, { .fields = { 72, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -160, 88, 650, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 136 } }, -160, -48, 650, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, -144, -112, 650, { .fields = { 112, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 80, 24 } }, -128, -120, 650, { .fields = { 24, 32 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_dryfield_parking_lot_8017EE68[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 7, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_dryfield_parking_lot_8017EE80[21] = {
    { 142, 0x3FC0, { .fields = { 40, 24 } }, 120, -80, 528, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, 80, -56, 531, { .fields = { 120, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, 120, -56, 993, { .fields = { 120, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, 80, -16, 525, { .fields = { 120, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, 120, -16, 751, { .fields = { 120, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, 80, 24, 518, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, 120, 24, 510, { .fields = { 80, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, 80, 64, 515, { .fields = { 80, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, 120, 64, 507, { .fields = { 80, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, 16, -120, 1012, { .fields = { 112, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 16, -72, 1012, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 16, -24, 1012, { .fields = { 80, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 16, 24, 1012, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 64, 24, 1012, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 64, -24, 1012, { .fields = { 80, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 64, -72, 1012, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 64, -120, 1012, { .fields = { 32, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, 112, -120, 1012, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 112, -72, 1012, { .fields = { 32, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 112, -24, 1012, { .fields = { 32, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 112, 24, 925, { .fields = { 32, 96 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_dryfield_parking_lot_8017F024[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 9, 0, 0, { 1, 0 } },
    { 9, 12, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_dryfield_parking_lot_8017F044[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtRec D_dryfield_parking_lot_8017F054[7] = {
    { { .empty = D_dryfield_parking_lot_8017E9FC }, D_dryfield_parking_lot_8017E9FC, NULL },
    { { .elements = D_dryfield_parking_lot_8017EA0C }, D_dryfield_parking_lot_8017EAD4, D_dryfield_parking_lot_8017EAF4 },
    { { .elements = D_dryfield_parking_lot_8017EB08 }, D_dryfield_parking_lot_8017ECE8, NULL },
    { { .elements = D_dryfield_parking_lot_8017ED00 }, D_dryfield_parking_lot_8017EDB4, NULL },
    { { .elements = D_dryfield_parking_lot_8017EDDC }, D_dryfield_parking_lot_8017EE68, NULL },
    { { .elements = D_dryfield_parking_lot_8017EE80 }, D_dryfield_parking_lot_8017F024, NULL },
    { { .empty = D_dryfield_parking_lot_8017F044 }, D_dryfield_parking_lot_8017F044, NULL },
};

GpObj4C D_dryfield_parking_lot_8017F0A8[10] = {
    { NULL, NULL, NULL, { -2698, -2736, -1594, 0 }, { { -2084, -3856, 2134, 0 }, { 2085, -3856, -2134, 0 }, { -2084, 3856, 2134, 0 }, { 2085, 3856, -2134, 0 } }, { -2935, 0, -2867, 0 }, { 0, 0, 4096, 0 }, 4857, 0, 2, 3, 1, 0 },
    { NULL, NULL, NULL, { -2816, -2592, -1600, 0 }, { { 2078, -3616, -2142, 0 }, { -2092, -3616, 2127, 0 }, { 2078, 3616, -2142, 0 }, { -2092, 3616, 2127, 0 } }, { 2941, 0, 2873, 0 }, { 0, 0, 4096, 0 }, 4664, 0, 3, 2, 1, 0 },
    { NULL, NULL, NULL, { -2560, -2656, 1664, 0 }, { { 1854, -3680, -2342, 0 }, { -1877, -3680, 2312, 0 }, { 1854, 3680, -2342, 0 }, { -1877, 3680, 2312, 0 } }, { 3203, 0, 2568, 0 }, { 0, 0, 4096, 0 }, 4720, 0, 4, 3, 1, 0 },
    { NULL, NULL, NULL, { -2528, -2720, 1760, 0 }, { { -1873, -3744, 2314, 0 }, { 1857, -3744, -2338, 0 }, { -1873, 3744, 2314, 0 }, { 1857, 3744, -2338, 0 } }, { -3200, 0, -2566, 0 }, { 0, 0, 4096, 0 }, 4775, 0, 3, 4, 1, 0 },
    { NULL, NULL, NULL, { 831, -2528, 2015, 0 }, { { -694, -3552, 2899, 0 }, { 690, -3552, -2903, 0 }, { -694, 3552, 2899, 0 }, { 690, 3552, -2903, 0 } }, { -3985, 0, -951, 0 }, { 0, 0, 4096, 0 }, 4636, 0, 4, 5, 1, 0 },
    { NULL, NULL, NULL, { 798, -2416, 2046, 0 }, { { 690, -3440, -2903, 0 }, { -694, -3440, 2899, 0 }, { 690, 3440, -2903, 0 }, { -694, 3440, 2899, 0 } }, { 3998, 0, 953, 0 }, { 0, 0, 4096, 0 }, 4550, 0, 5, 4, 1, 0 },
    { NULL, NULL, NULL, { 8191, -2320, 2527, 0 }, { { -34, -3344, -2982, 0 }, { 35, -3344, 2983, 0 }, { -34, 3344, -2982, 0 }, { 35, 3344, 2983, 0 } }, { 4106, 0, -48, 0 }, { 0, 0, 4096, 0 }, 4463, 0, 7, 6, 1, 0 },
    { NULL, NULL, NULL, { 8223, -2256, 2399, 0 }, { { 35, -3280, 2983, 0 }, { -34, -3280, -2982, 0 }, { 35, 3280, 2983, 0 }, { -34, 3280, -2982, 0 } }, { -4098, 0, 47, 0 }, { 0, 0, 4096, 0 }, 4404, 0, 6, 7, 1, 0 },
    { NULL, NULL, NULL, { 4048, -2912, 3200, 0 }, { { -500, -3856, 3222, 0 }, { 501, -3856, -3222, 0 }, { -500, 3856, 3222, 0 }, { 501, 3856, -3222, 0 } }, { -4059, 0, -631, 0 }, { 0, 0, 4096, 0 }, 5042, 0, 5, 6, 1, 0 },
    { NULL, NULL, NULL, { 3968, -2848, 2864, 0 }, { { 396, -3856, -2554, 0 }, { -395, -3856, 2554, 0 }, { 396, 3856, -2554, 0 }, { -395, 3856, 2554, 0 } }, { 4054, 0, 627, 0 }, { 0, 0, 4096, 0 }, 4636, 0, 6, 5, 129, 0 },
};

GpObj4C D_dryfield_parking_lot_8017F3A0[11] = {
    { NULL, NULL, NULL, { 0x2870, -48, 2976, 0 }, { { -560, 0, -1024, 0 }, { 560, 0, -1024, 0 }, { -560, 0, 1024, 0 }, { 560, 0, 1024, 0 } }, { 0, 4110, 0, 0 }, { -4096, 0, 0, 0 }, 1166, 0, 2, 19, 2, 0 },
    { NULL, NULL, NULL, { 3088, -48, 3744, 0 }, { { 848, 0, -560, 0 }, { 848, 0, 560, 0 }, { -848, 0, -560, 0 }, { -848, 0, 560, 0 } }, { 0, 4098, 0, 0 }, { 0, 0, -4096, 0 }, 1015, 0, 17, 33, 2, 0 },
    { NULL, NULL, NULL, { -560, -48, 192, 0 }, { { 720, 0, -560, 0 }, { 720, 0, 560, 0 }, { -720, 0, -560, 0 }, { -720, 0, 560, 0 } }, { 0, 4106, 0, 0 }, { 0, 0, 4096, 0 }, 911, 0, 16, 49, 2, 0 },
    { NULL, NULL, NULL, { -6736, -48, -640, 0 }, { { 656, 0, -560, 0 }, { 656, 0, 560, 0 }, { -656, 0, -560, 0 }, { -656, 0, 560, 0 } }, { 0, 4097, 0, 0 }, { 0, 0, -4096, 0 }, 861, 0, 18, 65, 2, 0 },
    { NULL, NULL, NULL, { 4752, -48, 2032, 0 }, { { 368, 0, -224, 0 }, { 368, 0, 224, 0 }, { -368, 0, -224, 0 }, { -368, 0, 224, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, 4096, 0 }, 430, 1, 52, 128, 2, 0 },
    { NULL, NULL, NULL, { 4640, -992, 1632, 0 }, { { -560, 1024, 0, 0 }, { 560, 1024, 0, 0 }, { -560, -1024, 0, 0 }, { 560, -1024, 0, 0 } }, { 0, 0, 4110, 0 }, { 0, 0, 4096, 0 }, 1166, 0, 29, 81, 2, 0 },
    { NULL, NULL, NULL, { -1104, -64, 3024, 0 }, { { 432, 0, -1024, 0 }, { 432, 0, 1024, 0 }, { -432, 0, -1024, 0 }, { -432, 0, 1024, 0 } }, { 0, 4097, 0, 0 }, { 4091, 0, 201, 0 }, 1108, 2, 4, 0, 2, 0 },
    { NULL, NULL, NULL, { 4672, -512, 1408, 0 }, { { 512, 0, -112, 0 }, { 512, 0, 112, 0 }, { -512, 0, -112, 0 }, { -512, 0, 112, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, -4096, 0 }, 523, 0x8101, 50, 0, 2, 0 },
    { NULL, NULL, NULL, { -2048, -64, 2448, 0 }, { { 1232, 0, -432, 0 }, { 1232, 0, 432, 0 }, { -1232, 0, -432, 0 }, { -1232, 0, 432, 0 } }, { 0, 4119, 0, 0 }, { 0, 0, -4096, 0 }, 1305, 2, 4, 0, 2, 0 },
    { NULL, NULL, NULL, { -7128, -64, -1968, 0 }, { { 1704, 0, -320, 0 }, { 1480, 0, 736, 0 }, { -1592, 0, -768, 0 }, { -1592, 0, 352, 0 } }, { 0, 4100, 0, 0 }, { 401, 0, 4076, 0 }, 1764, 2, 5, 0, 2, 0 },
    { NULL, NULL, NULL, { -5520, -64, -3104, 0 }, { { 960, 0, -1680, 0 }, { 224, 0, 1680, 0 }, { -224, 0, -1680, 0 }, { -960, 0, 1680, 0 } }, { 0, 4096, 0, 0 }, { 4017, 0, 799, 0 }, 1932, 2, 5, 0, 130, 0 },
};

GpObj3A D_dryfield_parking_lot_8017F6E4[2] = {
    { NULL, NULL, { -416, -1872, -3328, 0 }, { { 1078, 2896, -3014, 0 }, { 1078, -2896, -3014, 0 }, { -1079, 2896, 3013, 0 }, { -1079, -2896, 3013, 0 } }, { 3859, 0, 1381, 0 }, { -38, 16 }, 1, 0 },
    { NULL, NULL, { -6544, -1728, 2800, 0 }, { { 2064, 2896, -2704, 0 }, { 2064, -2896, -2704, 0 }, { -2064, 2896, 2704, 0 }, { -2064, -2896, 2704, 0 } }, { 3260, 0, 2488, 0 }, { 111, 17 }, 129, 0 },
};

GpPointLight D_dryfield_parking_lot_8017F75C[7] = {
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, -6980, 0 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 3112, 2949, 2785, { 0, 0 } }, 0x186A0, 0x186A0 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -7480, -2210, -4200 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 4915, 4915, 4915, { 0, 0 } }, 4500, 6000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 1520, -980, 2900 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 4096, 4096, 4096, { 0, 0 } }, 1000, 2000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 3220, -980, 2900 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 4096, 4096, 4096, { 0, 0 } }, 1000, 2000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -2980, -980, 1500 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 4096, 4096, 4096, { 0, 0 } }, 1000, 2000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -180, -980, 2900 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 4096, 4096, 4096, { 0, 0 } }, 1000, 2000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -4180, -980, -100 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 4096, 4096, 4096, { 0, 0 } }, 1000, 3000 },
};

GpRoomCoordSet D_dryfield_parking_lot_8017F9FC[1] = {
    { 0, NULL, 7, D_dryfield_parking_lot_8017F75C, 0, NULL },
};

GpAreaTmdRec D_dryfield_parking_lot_8017FA14[1] = {
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_dryfield_parking_lot_8017FA20[2] = {
    { 1, 1, 3, 0, { 0, 0 }, &D_8014D8A4 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_dryfield_parking_lot_8017FA38[3] = {
    { 1, 1, 3, 0, { 0, 0 }, &D_8014D8A4 },
    { 25, 25, 2, 0, { 0, 0 }, D_801679A8 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_dryfield_parking_lot_8017FA5C[2] = {
    { 22, 22, 3, 0, { 0, 0 }, D_80154188 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaVariant D_dryfield_parking_lot_8017FA74[13] = {
    { NULL, NULL },
    { D_map_dryfield_8017B1C4, D_dryfield_parking_lot_8017FA14 },
    { D_map_dryfield_8017B1D4, D_dryfield_parking_lot_8017FA20 },
    { D_map_dryfield_8017B204, D_dryfield_parking_lot_8017FA38 },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { D_map_dryfield_8017B284, D_dryfield_parking_lot_8017FA5C },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
};

s32 D_dryfield_parking_lot_8017FADC[3] = {
    0x10000049,
    0x1000004B,
    0x10000049,
};

s32 D_dryfield_parking_lot_8017FAE8[3] = {
    0x1000003D,
    0x1000003F,
    0x1000003D,
};

s32 D_dryfield_parking_lot_8017FAF4[3] = {
    0x10000051,
    0x10000053,
    0x10000055,
};

GpRoomParamRec D_dryfield_parking_lot_8017FB00[1] = {
    { 0, 0, 1, 0, NULL },
};

GpRoomParamRec D_dryfield_parking_lot_8017FB08[1] = {
    { 0, 0, 1, 0, D_dryfield_parking_lot_8017FADC },
};

GpRoomParamRec D_dryfield_parking_lot_8017FB10[1] = {
    { 0, 0, 1, 0, D_dryfield_parking_lot_8017FAE8 },
};

GpRoomParamRec D_dryfield_parking_lot_8017FB18[1] = {
    { 0, 0, 1, 0, D_dryfield_parking_lot_8017FAF4 },
};

GpRoomParamRec D_dryfield_parking_lot_8017FB20[1] = {
    { 0, 1, 0, 0, NULL },
};

GpRoomParamRec D_dryfield_parking_lot_8017FB28[1] = {
    { 0, 0, 1, 0, D_dryfield_parking_lot_8017FADC },
};

GpRoomParamRec * D_dryfield_parking_lot_8017FB30[8] = {
    D_dryfield_parking_lot_8017FB00,
    D_dryfield_parking_lot_8017FB20,
    D_dryfield_parking_lot_8017FB10,
    D_dryfield_parking_lot_8017FB18,
    D_dryfield_parking_lot_8017FB00,
    D_dryfield_parking_lot_8017FB08,
    D_dryfield_parking_lot_8017FB28,
    D_dryfield_parking_lot_8017FB00,
};

RoomEventMsg D_dryfield_parking_lot_8017FB50 = { 0 };

u8 D_dryfield_parking_lot_8017FB58[4] = {
    0,
    156,
    190,
    128,
};

RoomEventReq D_dryfield_parking_lot_8017FB5C = { 0 };

/// The room's event gate. A request whose flag nibble is already set (or clear,
/// for a negative `flagId`) answers 1. One whose prerequisite item is missing
/// runs the request's CAP command `field_4` and answers 0. Otherwise the
/// message and request are latched, the nibble is written, the event task is
/// spawned and the answer is 2. A non-zero `field_5` on the message only
/// reports the answer, with none of the side effects.
static s32 func_dryfield_parking_lot_8017D5E8(RoomEventReq* req, RoomEventMsg* msg)
{
    s32 flag;
    s32 id;
    s32 mode;
    s32 got;
    s32 ret;
    s32 neg;

    flag                            = req->flagId;
    D_dryfield_parking_lot_8017FB58[0] = 0;
    neg                             = flag < 0;
    got                             = (s16)flag;
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
                D_dryfield_parking_lot_8017FB50 = *msg;
                D_dryfield_parking_lot_8017FB5C = *req;
                id                              = req->flagId;
                mode                            = 1;
                if (id < 0) {
                    id   = -id;
                    mode = 0;
                }
                GameFlag_SetNibble(id, mode);
                Task_SpawnFromTable(&D_dryfield_parking_lot_8017DBF8, 0, 0, 0);
                D_dryfield_parking_lot_8017FB58[0] = 1;
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

/// The event task the gate spawns. State 0 runs the latched request's CAP
/// command; states 1-4 play its two sounds in turn (`field_8`, then
/// `field_C`), each skipped when zero and waited on until its voice falls
/// silent; state 5 writes the latched message's destination into the save
/// data and hands over to task type 0x11.
void func_dryfield_parking_lot_8017D74C(Task* task)
{
    switch (task->state) {
        case 0:
            Gp_StateF0.field_4 = 1;
            Gp_MsgPlayerWeapon(0);
            Gp_RunCapCmd1(D_dryfield_parking_lot_8017FB5C.field_0);
            if (D_dryfield_parking_lot_8017FB5C.field_8 != 0) {
                SndEvt_EnqueueType6(D_dryfield_parking_lot_8017FB5C.field_8, 0, 0);
                task->state++;
            } else {
                task->state = 2;
            }
            break;
        case 1:
            if (SndVoice_HasActiveId(D_dryfield_parking_lot_8017FB5C.field_8) == 0) {
                task->state++;
            }
            break;
        case 2:
            task->state++;
            break;
        case 3:
            if (D_dryfield_parking_lot_8017FB5C.field_C != 0) {
                SndEvt_EnqueueType6(D_dryfield_parking_lot_8017FB5C.field_C, 0, 0);
                task->state++;
            } else {
                task->state = 5;
            }
            break;
        case 4:
            if (SndVoice_HasActiveId(D_dryfield_parking_lot_8017FB5C.field_C) == 0) {
                task->state++;
            }
            break;
        case 5:
            SndEvt_EnqueueType7(0x80000000, 0);
            gDisplayState.roomVariant   = 1;
            Mc_SaveData[0].state.at4.loc.area = D_dryfield_parking_lot_8017FB50.prefix.packed;
            Mc_SaveData[0].state.at4.loc.warp = D_dryfield_parking_lot_8017FB50.field_2;
            Mc_SaveData[0].state.at4.loc.room = (u8)D_dryfield_parking_lot_8017FB50.field_3;
            Task_Spawn(0, 0x11, 0, 0);
            taskKill(task);
            break;
    }
}

/// Handler for message 0x13EE in the room's message table: the room's two
/// reports and its two events. The incoming record is first copied to `out`.
///
/// Messages 2 and 0x1D answer in `out->field_3` (only when `field_5` is clear):
/// message 2 gives nibble 0x61 plus one while nibble 0x7A is under 4, and 3
/// once it is not; message 0x1D gives 1 while nibble 0x61 is clear and 3 once
/// it is set.
///
/// Messages 0x11 and 0x12 are the events: each builds a request for the
/// room's event gate `func_dryfield_parking_lot_8017D5E8` - message 0x11 on
/// nibble 0x40 with item 0x12, message 0x12 on nibble 0x35 with item 0x10.
/// When the gate reports the event fired, 0x11 applies the area records
/// `D_dryfield_night_parking_lot_8018155C` and sets nibbles 0x46 and 0x97, while 0x12 sets item-seen bit
/// 0x110. Any other message returns 1.
s32 func_dryfield_parking_lot_8017D8BC(Task* task, s32 msgId, RoomEventMsg * msg, RoomEventMsg * out)
{
    RoomEventReq req;
    s32          ret;
    s32          val;
    s32          n;

    *out = *msg;
    if ((msg->prefix.packed == 2) && (msg->field_5 == 0)) {
        n = GameFlag_GetNibble(0x7A);
        if (n >= 4) {
            val = 3;
        } else {
            val = GameFlag_GetNibble(0x61) + 1;
        }
        out->field_3 = val;
    }
    if ((msg->prefix.packed == 0x1D) && (msg->field_5 == 0)) {
        n = GameFlag_GetNibble(0x61);
        if (n == 0) {
            n = 1;
        } else {
            n = 3;
        }
        out->field_3 = n;
    }
    if (msg->prefix.packed == 0x11) {
        req.field_0 = 6;
        req.field_4 = 1;
        req.field_8 = Gp_PackStageSndId(0x520F000B);
        req.field_C = Gp_PackStageSndId(0x520F0007);
        req.flagId  = 0x40;
        req.itemId  = 0x12;
        ret         = func_dryfield_parking_lot_8017D5E8(&req, out);
        if (ret == 0) {
            ret = 2;
        }
        if (D_dryfield_parking_lot_8017FB58[0] != 0) {
            Gp_ApplyAreaRecs(D_dryfield_night_parking_lot_8018155C);
            GameFlag_SetNibble(0x46, 1);
            GameFlag_SetNibble(0x97, 1);
        }
    } else if (msg->prefix.packed == 0x12) {
        req.field_0 = 3;
        req.field_4 = 2;
        req.field_8 = Gp_PackStageSndId(0x520F000B);
        req.field_C = Gp_PackStageSndId(0x520F0007);
        req.flagId  = 0x35;
        req.itemId  = 0x10;
        ret         = func_dryfield_parking_lot_8017D5E8(&req, out);
        if (D_dryfield_parking_lot_8017FB58[0] != 0) {
            Gp_SetItemSeenBit(0x110, 1);
        }
    } else {
        return 1;
    }
    return ret;
}

/// Handler for message 0x13F2 in the room's message table, keyed by `arg2`:
/// point 9 plays stage sound 0x520F0009 and point 10 plays 0x520F000A. Always
/// returns 0.
s32 func_dryfield_parking_lot_8017DAA0(Task* arg0, s32 arg1, s32 arg2, GpMessageArg arg3)
{
    switch (arg2) {
        case 9:
            Gp_EnqueueStageSnd6(0x520F0009, 0, 0);
            break;
        case 10:
            Gp_EnqueueStageSnd6(0x520F000A, 0, 0);
            break;
    }
    return 0;
}

/// Handler for message 0x13F1 in the room's message table: does nothing and
/// returns 0.
s32 func_dryfield_parking_lot_8017DAF0(Task* task, s32 msgId, GpMessageArg arg2, GpMessageArg arg3)
{
    return 0;
}

/// Handler for message 0x13F0 in the room's message table: does nothing and
/// returns 0.
s32 func_dryfield_parking_lot_8017DAF8(Task* task, s32 msgId, GpMessageArg arg2, GpMessageArg arg3)
{
    return 0;
}

/// Handler for message 0x13EF in the room's message table: does nothing and
/// returns 0.
s32 func_dryfield_parking_lot_8017DB00(Task* task, s32 msgId, GpMessageArg arg2, GpMessageArg arg3)
{
    return 0;
}

/// Room entry task state 0: parks the room's message table in `Task::msgTable`,
/// publishes the task in pointer slot 7 and advances the state.
static void func_dryfield_parking_lot_8017DB08(Task* task)
{
    task->msgTable = D_dryfield_parking_lot_8017DC04;
    Game_SetPtrSlot(task, 7);
    task->state = (s32)(task->state + 1);
}

/// Room entry task state 1: does nothing, and nothing here advances the state.
static void func_dryfield_parking_lot_8017DB4C(Task* task)
{
}

/// The room entry task's states: set up, idle, then `taskKill`.
static const TaskFuncTable3 D_dryfield_parking_lot_8017D5DC = {
    { func_dryfield_parking_lot_8017DB08, func_dryfield_parking_lot_8017DB4C, taskKill },
};

/// The room entry task: copies the three-state table to the stack and runs the
/// entry the task's state selects.
void func_dryfield_parking_lot_8017DB54(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_parking_lot_8017D5DC;
    sp.funcs[task->state](task);
}

/// Publishes the value the current camera view maps to: stores
/// `D_dryfield_parking_lot_8017DC34[view - 1]` into `Gp_State1C`'s
/// `roomEffectMode`. Nothing in the room calls it; gameplay's data holds its
/// address.
void func_dryfield_parking_lot_8017DBAC(Task* unused)
{
    Gp_State1C->roomEffectMode = D_dryfield_parking_lot_8017DC34[(Gp_GetViewIndex() & 0xFF) - 1];
}
