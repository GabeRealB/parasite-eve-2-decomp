#include "rooms/shelter_b2_operating_room.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "common.h"
#include "gte.h"

#include "actors/task_tables.h"

#include "gameplay/actor_render.h"
#include "gameplay/area.h"
#include "gameplay/areaplace.h"
#include "gameplay/captions.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/direction_input.h"
#include "gameplay/display.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/effects.h"
#include "gameplay/items.h"
#include "gameplay/light.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"
#include "gameplay/world_coords.h"
#include "gameplay/world_state.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gameflag.h"
#include "main/gamemain.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "mapui/map_shelter.h"

#include "overlay.h"

#include "rooms/room.h"

#include "rooms/room_common.h"

#define D_shelter_b2_operating_room_80180ABC (D_shelter_b2_operating_room_801809BC + 32)
#define D_shelter_b2_operating_room_80180ADC (D_shelter_b2_operating_room_801809BC + 36)
#define D_shelter_b2_operating_room_80180B44 (D_shelter_b2_operating_room_801809BC + 49)
#define D_shelter_b2_operating_room_80180B5C (D_shelter_b2_operating_room_801809BC + 52)
#define D_shelter_b2_operating_room_80180B6C (D_shelter_b2_operating_room_801809BC + 54)

// Preserve the following nonzero bytes with this scalar's storage.
// No separate references identify them; their role (including padding) is unresolved.
extern u8 D_shelter_b2_operating_room_80184234[4];

/// Spawn descriptor of the exit gate's transition task.
extern TaskDesc D_shelter_b2_operating_room_80180904;

/// Descriptor of the room's own event task, which the message handler spawns.
extern TaskDesc D_shelter_b2_operating_room_80180910;

/// The room's message table, which the cap scripts index.
extern GpMsgEntry D_shelter_b2_operating_room_8018091C[];

/// Point pairs and points the view task draws its glows at, per view.

/// Per-colour channel shifts for the halo task, indexed by the colour its
/// spawn argument selects.
extern s16 D_shelter_b2_operating_room_80180BBC[][3];

/// Spawn argument of the helper task 0x31 the room's event task starts.
extern RoomFadeStorage D_shelter_b2_operating_room_80184214;

/// The message and request of the exit the gate last accepted, latched for
/// the transition task, and the flag saying the gate spawned it.
extern RoomEventMsg D_shelter_b2_operating_room_8018421C;
// Only the leading value has established accesses. Preserve the following
// zero bytes in this allocation; trailing fields versus TU padding remains
// unresolved (see the local actors/rooms data review).
typedef struct {
    u8 value;
    u8 retained[7];
} ShelterB2OperatingRoomStorage4224;
STATIC_ASSERT_SIZEOF(ShelterB2OperatingRoomStorage4224, 8);

extern ShelterB2OperatingRoomStorage4224 D_shelter_b2_operating_room_80184224;
// Only the leading value has established accesses. Preserve the following
// zero bytes in this allocation; trailing fields versus TU padding remains
// unresolved (see the local actors/rooms data review).
typedef struct {
    RoomEventReq value;
    u8           retained[12];
} ShelterB2OperatingRoomStorage4238;
STATIC_ASSERT_SIZEOF(ShelterB2OperatingRoomStorage4238, 32);

extern ShelterB2OperatingRoomStorage4238 D_shelter_b2_operating_room_80184238;

/// The message and event the message handler latched for the room's event
/// task, and the flag saying the handler spawned it.
extern RoomEventMsg     D_shelter_b2_operating_room_8018422C;
extern RoomLatchedEvent D_shelter_b2_operating_room_80184258;

static void func_shelter_b2_operating_room_8017E118(SVECTOR* arg0, s32 arg1, s32 arg2);
static void func_shelter_b2_operating_room_8017E95C(SVECTOR* arg0, s32 arg1, s32 arg2);
static void func_shelter_b2_operating_room_8017F478(GfxCoord* arg0, s32 arg1, s32 arg2, s32 arg3);
static void func_shelter_b2_operating_room_8017F6FC(GfxCoord* arg0, s32 arg1, s32 arg2, u8* rgb);
static void func_shelter_b2_operating_room_8017FB20(GfxCoord* arg0, s32 arg1, u8* rgb);
static void func_shelter_b2_operating_room_80180060(GfxCoord* coord, s16 size);
static void func_shelter_b2_operating_room_8018058C(GfxCoord* arg0, s32 arg1);

// Indexed views below share one contiguous table.
extern TaskDesc D_801575F0;

s32  func_shelter_b2_operating_room_8017DA94(Task*, s32, RoomEventMsg*, RoomEventMsg*);
s32  func_shelter_b2_operating_room_8017DC9C(Task*, s32, TaskMessageArg, TaskMessageArg);
s32  func_shelter_b2_operating_room_8017DCA4(Task*, s32, s32, TaskMessageArg);
s32  func_shelter_b2_operating_room_8017DD0C(Task*, s32, TaskMessageArg, TaskMessageArg);
void func_shelter_b2_operating_room_8017D78C(Task*);
void func_shelter_b2_operating_room_8017D8FC(Task*);

TaskDesc D_shelter_b2_operating_room_80180904 = { 0, 32, func_shelter_b2_operating_room_8017D78C, { .model = NULL } };

TaskDesc D_shelter_b2_operating_room_80180910 = { 0, 32, func_shelter_b2_operating_room_8017D8FC, { .model = NULL } };

GpMsgEntry D_shelter_b2_operating_room_8018091C[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, func_shelter_b2_operating_room_8017DA94 },
    { 5105, func_shelter_b2_operating_room_8017DC9C },
    { 5103, func_shelter_b2_operating_room_8017DD0C },
    { 5104, func_shelter_b2_operating_room_8017DCA4 },
    { 0x7FFFFFFF, NULL },
};

SVECTOR D_shelter_b2_operating_room_80180944[15] = {
    { 4950, -1750, 2570, 0 },
    { 5170, -1750, 2570, 0 },
    { 5340, -1750, 2700, 0 },
    { 5410, -1750, 2900, 0 },
    { 5340, -1750, 3100, 0 },
    { 5170, -1750, 3230, 0 },
    { 4950, -1750, 3230, 0 },
    { 4770, -1750, 3100, 0 },
    { 4690, -1750, 2900, 0 },
    { 4770, -1750, 2700, 0 },
    { 4900, -1750, 2900, 0 },
    { 5200, -1750, 2900, 0 },
    { 3360, 200, 3525, 0 },
    { 3880, 200, 3525, 0 },
    { 4600, 200, 3525, 0 },
};

SVECTOR D_shelter_b2_operating_room_801809BC[64] = {
    { 5235, 200, 3525, 0 },
    { 5855, 200, 3525, 0 },
    { 6475, 200, 3525, 0 },
    { 7100, 200, 3525, 0 },
    { 3360, 200, 2925, 0 },
    { 3880, 200, 2925, 0 },
    { 4600, 200, 2925, 0 },
    { 5235, 200, 2925, 0 },
    { 5855, 200, 2925, 0 },
    { 6475, 200, 2925, 0 },
    { 7100, 200, 2925, 0 },
    { 3360, 200, 2310, 0 },
    { 3880, 200, 2310, 0 },
    { 4600, 200, 2310, 0 },
    { 5235, 200, 2310, 0 },
    { 5855, 200, 2310, 0 },
    { 6475, 200, 2310, 0 },
    { 7100, 200, 2310, 0 },
    { 3885, -2245, 4640, 0 },
    { 4865, -2245, 4640, 0 },
    { 5845, -2245, 4640, 0 },
    { 6825, -2245, 4640, 0 },
    { 3885, -2245, 960, 0 },
    { 4865, -2245, 960, 0 },
    { 5845, -2245, 960, 0 },
    { 6825, -2245, 960, 0 },
    { 2560, -2245, 3670, 0 },
    { 2560, -2245, 2800, 0 },
    { 2560, -2245, 1930, 0 },
    { 8140, -2245, 3670, 0 },
    { 8140, -2245, 2800, 0 },
    { 8140, -2245, 1930, 0 },
    { -150, -1820, 5300, 0 },
    { 460, -1820, 5300, 0 },
    { 360, -2245, 3825, 0 },
    { 720, -2245, 3825, 0 },
    { 360, -2245, 1345, 0 },
    { 720, -2245, 1345, 0 },
    { 390, -2025, 295, 0 },
    { 690, -2025, 295, 0 },
    { 1690, -2130, 1810, 0 },
    { 1690, -2130, 1425, 0 },
    { 2310, -1980, 1625, 0 },
    { 2310, -1980, 1390, 0 },
    { 1875, -1825, 1090, 0 },
    { 2115, -1825, 1090, 0 },
    { 2980, -1765, 335, 0 },
    { 4365, -1765, 335, 0 },
    { 2980, -995, 335, 0 },
    { 3672, -1380, 335, 0 },
    { 8560, -1980, 2310, 0 },
    { 8560, -1980, 1925, 0 },
    { 9990, -2300, 5940, 0 },
    { 10375, -2300, 5940, 0 },
    { 10145, -2300, 45, 0 },
    { 10525, -2300, 45, 0 },
    { 8915, -2720, 4090, 0 },
    { 8915, -2720, 3605, 0 },
    { 8915, -2720, 2125, 0 },
    { 8915, -2720, 1640, 0 },
    { 11600, -2720, 4090, 0 },
    { 11600, -2720, 3605, 0 },
    { 11600, -2720, 2125, 0 },
    { 11600, -2720, 1640, 0 },
};

s16 D_shelter_b2_operating_room_80180BBC[2][3] = {
    { 1, 0, 0 },
    { 0, 1, 0 },
};

u8* D_shelter_b2_operating_room_80180BC8[1] = {
    D_8010CAF8,
};

GpViewCountRec D_shelter_b2_operating_room_80180BCC[1] = {
    { { .bytes = { 7, 0 } } },
};

GpWarpRec D_shelter_b2_operating_room_80180BD0[3] = {
    { { .words = { 0, 0x27D8, 0, 400 } }, { 0, 0, 0, 0 }, { .words = { 0, 0x27D8, 0, 400 } }, { 0, 0, 0, 0 }, 0x541D0002, 0x541D0001, 0x541D0008, 2, 0, 456 },
    { { .words = { 2048, 0x27D8, 0, 5600 } }, { 0, 0, 0, 0 }, { .words = { 2048, 0x27D8, 0, 5600 } }, { 0, 0, 0, 0 }, 0x541D0004, 0x541D0003, 0, 3, 0, 458 },
    { { .words = { 0, 496, 0, 1017 } }, { 0, 0, 0, 0 }, { .words = { 0, 496, 0, 1017 } }, { 0, 0, 0, 0 }, 0x541D0006, 0x541D0005, 0, 7, 0, 0 },
};

SVECTOR D_shelter_b2_operating_room_80180C78[18] = {
#include "assets/shelter_b2_operating_room_collision_03DA4_normals.inc"
};

SVECTOR D_shelter_b2_operating_room_80180D08[99] = {
#include "assets/shelter_b2_operating_room_collision_03DA4_verts.inc"
};

GpGridFace D_shelter_b2_operating_room_80181020[42] = {
#include "assets/shelter_b2_operating_room_collision_03DA4_faces.inc"
};

s16 D_shelter_b2_operating_room_80181218[150] = {
#include "assets/shelter_b2_operating_room_collision_03DA4_cells.inc"
};

#define GRID_CELL(i) (&D_shelter_b2_operating_room_80181218[i])
s16* D_shelter_b2_operating_room_80181344[8] = {
#include "assets/shelter_b2_operating_room_collision_03DA4_table.inc"
};
#undef GRID_CELL

GpGridParams D_shelter_b2_operating_room_80181364 = { NULL, D_shelter_b2_operating_room_80180C78, D_shelter_b2_operating_room_80180D08, D_shelter_b2_operating_room_80181020, D_shelter_b2_operating_room_80181344, 1000, 0, 4, 2, 4000, 42 };

GpViewRec D_shelter_b2_operating_room_80181388[12] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { -5500, 0x5208, -3500 } }, 380 },
    { { { { -4023, 0, -767 }, { 132, 4034, -694 }, { 755, -707, -3963 } }, { -9754, 492, -5270 } }, 230 },
    { { { { 3968, 0, -1013 }, { 80, 4083, 313 }, { 1010, -324, 3956 } }, { -9594, 639, -681 } }, 230 },
    { { { { 535, 0, 4060 }, { 2019, 3553, -266 }, { -3523, 2037, 464 } }, { -8388, 2849, -2161 } }, 257 },
    { { { { 698, 0, -4036 }, { -1979, 3569, -342 }, { 3517, 2008, 608 } }, { -2193, 2885, -2139 } }, 257 },
    { { { { 3937, 0, -1129 }, { -81, 4085, -282 }, { 1126, 293, 3926 } }, { -109, 1083, -705 } }, 230 },
    { { { { -3937, 0, -1129 }, { -62, 4089, 219 }, { 1127, 228, -3931 } }, { -342, 1051, -4981 } }, 230 },
    { { { { 108, 0, 4094 }, { 3896, 1258, -103 }, { -1257, 3897, 33 } }, { -5020, 1757, -3014 } }, 230 },
    { { { { 4095, 0, 0 }, { 0, 4017, -796 }, { 0, 796, 4017 } }, { -4888, 1408, -1622 } }, 261 },
    { { { { 15, 0, -4095 }, { 3773, 1592, 14 }, { 1592, -3773, 5 } }, { -4755, 952, -2617 } }, 257 },
    { { { { 3327, 0, 2388 }, { 304, 4062, -423 }, { -2368, 521, 3300 } }, { -7528, 1726, -67 } }, 415 },
    { { { { 16, 0, 4095 }, { 3674, 1809, -14 }, { -1809, 3674, 7 } }, { -5788, 2908, -3077 } }, 264 },
};

SpriteBatch D_shelter_b2_operating_room_80181538[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_shelter_b2_operating_room_80181548[25] = {
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 144, 88, 429, { .fields = { 112, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 104, 88, 496, { .fields = { 72, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 136, 88, 470, { .fields = { 104, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 128, 72, 492, { .fields = { 72, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 144, 16, 503, { .fields = { 64, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 72 } }, 136, -56, 523, { .fields = { 88, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 64 } }, 128, -120, 550, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 120, -80, 713, { .fields = { 104, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 128, -80, 694, { .fields = { 80, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 72 } }, 104, -40, 959, { .fields = { 80, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 104, 32, 918, { .fields = { 32, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 96, 64, 881, { .fields = { 88, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 88, 64, 881, { .fields = { 120, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 80 } }, 80, -120, 971, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 64, -120, 1100, { .fields = { 64, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 104 } }, 96, -40, 981, { .fields = { 120, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 104 } }, 88, -40, 1228, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 72, 56, 1167, { .fields = { 56, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 120, -120, 674, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 120, -72, 744, { .fields = { 56, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 112, -64, 797, { .fields = { 48, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 112, -56, 876, { .fields = { 40, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 104, -48, 960, { .fields = { 40, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 112, -120, 783, { .fields = { 80, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 104, -120, 879, { .fields = { 112, 80 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b2_operating_room_8018173C[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 7, 0, 0, { 1, 0 } },
    { 7, 18, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_shelter_b2_operating_room_8018175C[17] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -112, 88, 509, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -96, -120, 1054, { .fields = { 80, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 104 } }, -112, -120, 1084, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -96, 40, 1062, { .fields = { 64, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 104 } }, -112, -16, 1115, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 112 } }, -128, -16, 831, { .fields = { 104, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 112 } }, -120, -16, 920, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 104 } }, -120, -120, 923, { .fields = { 72, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 104 } }, -128, -120, 831, { .fields = { 80, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 120 } }, -136, -16, 771, { .fields = { 120, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 120 } }, -144, -16, 704, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 120 } }, -152, -16, 629, { .fields = { 112, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 120 } }, -160, -16, 556, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 104 } }, -136, -120, 771, { .fields = { 96, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 104 } }, -144, -120, 704, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 104 } }, -152, -120, 629, { .fields = { 88, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 104 } }, -160, -120, 556, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b2_operating_room_801818B0[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 17, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_shelter_b2_operating_room_801818C8[87] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -16, 40, 1121, { .fields = { 8, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, 0, -24, 1117, { .fields = { 32, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 16 } }, 0, -8, 1036, { .fields = { 24, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 16 } }, 0, 8, 932, { .fields = { 24, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 16 } }, 8, 24, 850, { .fields = { 8, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 16 } }, 16, 40, 776, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, 32, 56, 769, { .fields = { 56, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 64, 48, 784, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 32, 72, 809, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 24, 88, 809, { .fields = { 72, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 24, 56, 776, { .fields = { 16, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 16, 56, 839, { .fields = { 24, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 8, 40, 1033, { .fields = { 8, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 0, 24, 1170, { .fields = { 40, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -8, 32, 1137, { .fields = { 56, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, 96, 8, 800, { .fields = { 96, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, 64, 32, 834, { .fields = { 64, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, 96, 32, 775, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 72, 56, 917, { .fields = { 32, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, 72, 72, 896, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, 64, 88, 878, { .fields = { 32, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 88, 88, 866, { .fields = { 72, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -8, 24, 1197, { .fields = { 40, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, -24, -8, 1077, { .fields = { 24, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, -24, 0, 1036, { .fields = { 24, 16 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, -24, 8, 945, { .fields = { 32, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, -24, 24, 975, { .fields = { 56, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, -24, 40, 1024, { .fields = { 88, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 0, 24, 1009, { .fields = { 40, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, -40, 8, 791, { .fields = { 96, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, -32, 8, 775, { .fields = { 88, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -24, 16, 758, { .fields = { 56, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -16, 24, 694, { .fields = { 72, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -24, 56, 838, { .fields = { 48, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -16, 88, 867, { .fields = { 64, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -40, 88, 845, { .fields = { 64, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -88, -72, 1592, { .fields = { 8, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -104, -88, 1592, { .fields = { 48, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -120, -88, 1542, { .fields = { 48, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -128, -96, 1542, { .fields = { 64, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, -96, -32, 1527, { .fields = { 88, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -96, -8, 1527, { .fields = { 8, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -112, -8, 1527, { .fields = { 56, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -112, -32, 1527, { .fields = { 0, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 64 } }, 16, -32, 1199, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, 48, -32, 1043, { .fields = { 40, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 56, -16, 990, { .fields = { 56, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, 48, -16, 1000, { .fields = { 112, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, 40, -16, 1077, { .fields = { 120, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, 16, 0, 1020, { .fields = { 32, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 16 } }, 40, 8, 1175, { .fields = { 16, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, 40, 24, 1130, { .fields = { 80, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, 0, 64, 804, { .fields = { 32, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 8, 80, 814, { .fields = { 32, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 24, 80, 842, { .fields = { 24, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 0, 80, 850, { .fields = { 32, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 56, -120, 477, { .fields = { 64, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 40, -72, 521, { .fields = { 24, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, 40, -32, 555, { .fields = { 88, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 40, -8, 592, { .fields = { 96, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, 0, -40, 625, { .fields = { 80, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 24 } }, -48, -40, 746, { .fields = { 80, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -16, -72, 793, { .fields = { 16, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -40, -64, 738, { .fields = { 112, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 0, -80, 827, { .fields = { 120, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, 16, -80, 826, { .fields = { 104, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, 16, -56, 730, { .fields = { 64, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 0, -56, 741, { .fields = { 24, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 64 } }, -16, -32, 1239, { .fields = { 80, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -96, -8, 1467, { .fields = { 32, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, 56, -120, 1616, { .fields = { 56, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 16 } }, -8, -120, 1515, { .fields = { 24, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -72, -120, 1490, { .fields = { 0, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 56 } }, -120, -64, 1550, { .fields = { 96, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 24 } }, -136, -120, 1462, { .fields = { 64, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -88, -64, 1612, { .fields = { 104, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -88, -40, 1611, { .fields = { 32, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 56 } }, 56, -72, 1774, { .fields = { 64, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 56 } }, 16, -72, 1707, { .fields = { 88, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -128, -96, 1542, { .fields = { 16, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -96, -96, 1590, { .fields = { 0, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 16 } }, -40, -32, 1689, { .fields = { 32, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 24 } }, -48, -72, 1595, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 16 } }, -48, -48, 1657, { .fields = { 0, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, -48, -96, 1565, { .fields = { 120, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, -48, -120, 1517, { .fields = { 104, 144 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 88, 48 } }, -16, -112, 1621, { .fields = { 120, 0 } }, 128, 128, 128, 2 },
};

SpriteBatch D_shelter_b2_operating_room_80181F94[14] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 15, 0, 0, { 7, 0 } },
    { 15, 7, 0, 0, { 2, 0 } },
    { 22, 7, 0, 0, { 6, 0 } },
    { 29, 7, 0, 0, { 1, 0 } },
    { 36, 8, 0, 0, { 9, 0 } },
    { 44, 1, 0, 0, { 0, 0 } },
    { 45, 7, 0, 0, { 10, 0 } },
    { 52, 4, 0, 0, { 5, 0 } },
    { 56, 12, 0, 0, { 8, 0 } },
    { 68, 1, 0, 0, { 4, 0 } },
    { 69, 17, 0, 0, { 11, 0 } },
    { 86, 1, 0, 0, { 3, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_shelter_b2_operating_room_80182004[124] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -24, 56, 952, { .fields = { 96, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, -64, 56, 749, { .fields = { 24, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, -64, 48, 752, { .fields = { 16, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 8 } }, -64, 40, 804, { .fields = { 8, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 8 } }, -56, 32, 857, { .fields = { 8, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 8 } }, -56, 24, 888, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, -48, 16, 937, { .fields = { 8, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, -40, 8, 984, { .fields = { 16, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, -40, 0, 1037, { .fields = { 16, 16 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, -32, -8, 1096, { .fields = { 24, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 0, -8, 1119, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, 0, 24, 901, { .fields = { 64, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -16, 48, 928, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -8, 40, 1040, { .fields = { 120, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 0, 64, 964, { .fields = { 56, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 32 } }, -56, 64, 857, { .fields = { 56, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -80, 80, 645, { .fields = { 112, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -64, 72, 674, { .fields = { 112, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -48, 64, 693, { .fields = { 104, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -32, 64, 707, { .fields = { 96, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -16, 64, 708, { .fields = { 96, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -24, 56, 942, { .fields = { 112, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, -8, 64, 727, { .fields = { 104, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -8, 88, 730, { .fields = { 104, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, -24, 96, 852, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, -88, 104, 758, { .fields = { 64, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -96, 80, 750, { .fields = { 120, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, -16, 56, 762, { .fields = { 24, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -16, 48, 797, { .fields = { 40, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, -8, 40, 844, { .fields = { 48, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -8, 32, 871, { .fields = { 24, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, -8, 24, 922, { .fields = { 120, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 0, 16, 955, { .fields = { 32, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, 24, 24, 929, { .fields = { 56, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, 24, 40, 966, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, 24, 56, 1007, { .fields = { 72, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, 16, 72, 811, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, 16, 88, 846, { .fields = { 80, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, -16, 72, 812, { .fields = { 120, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, -16, 88, 881, { .fields = { 72, 16 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -40, 16, 920, { .fields = { 80, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, -96, 0, 881, { .fields = { 24, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, -104, 8, 836, { .fields = { 24, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, -104, 16, 824, { .fields = { 48, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -72, 24, 856, { .fields = { 64, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -64, 16, 911, { .fields = { 88, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -64, 8, 929, { .fields = { 64, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, -72, 8, 875, { .fields = { 72, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, -56, 24, 874, { .fields = { 48, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, -104, 40, 873, { .fields = { 40, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -96, 64, 895, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, -72, 40, 882, { .fields = { 32, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -72, 64, 885, { .fields = { 72, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -80, 32, 831, { .fields = { 24, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -104, 32, 835, { .fields = { 32, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -64, -24, 1086, { .fields = { 56, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -56, -24, 1105, { .fields = { 56, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -48, -32, 1166, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -48, -8, 1146, { .fields = { 24, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -64, 0, 1064, { .fields = { 24, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -40, 0, 1111, { .fields = { 32, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -40, 16, 1200, { .fields = { 48, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, -72, 24, 1206, { .fields = { 32, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, -64, 8, 1134, { .fields = { 40, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -8, 8, 1138, { .fields = { 64, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 40, 32, 1178, { .fields = { 104, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, 24, -32, 1079, { .fields = { 112, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 32, -32, 1041, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 40, -24, 964, { .fields = { 64, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 32, 8, 1120, { .fields = { 80, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 16, 24, 1171, { .fields = { 96, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -96, 56, 742, { .fields = { 16, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -112, 48, 739, { .fields = { 24, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -112, 64, 687, { .fields = { 0, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -80, 40, 798, { .fields = { 96, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, -64, 40, 768, { .fields = { 112, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, -88, 56, 739, { .fields = { 64, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -96, 48, 781, { .fields = { 40, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -56, 56, 860, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -72, 72, 707, { .fields = { 80, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -88, 72, 712, { .fields = { 80, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -104, 80, 690, { .fields = { 88, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 16 } }, -96, 104, 827, { .fields = { 40, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 16 } }, -104, 88, 753, { .fields = { 32, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, 96, -16, 1283, { .fields = { 48, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, 96, 0, 1324, { .fields = { 80, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, 112, -8, 1181, { .fields = { 88, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 96, 8, 1197, { .fields = { 24, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -72, -48, 712, { .fields = { 56, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -64, -48, 675, { .fields = { 96, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 32 } }, -48, -40, 669, { .fields = { 64, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -8, -40, 633, { .fields = { 80, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 8, -48, 709, { .fields = { 88, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 16, -48, 749, { .fields = { 72, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, 24, -48, 851, { .fields = { 88, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 40, -32, 851, { .fields = { 48, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, 24, -64, 844, { .fields = { 80, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -72, -56, 775, { .fields = { 64, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -48, -56, 713, { .fields = { 88, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, -32, -56, 710, { .fields = { 64, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, 16, -80, 968, { .fields = { 112, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 8, -56, 770, { .fields = { 24, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -56, -64, 862, { .fields = { 64, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -40, -64, 805, { .fields = { 40, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -8, -80, 993, { .fields = { 96, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -8, -120, 1013, { .fields = { 112, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 8, -16, 1639, { .fields = { 112, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 32 } }, 24, -120, 1494, { .fields = { 40, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, 80, -72, 1548, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, 120, -72, 1490, { .fields = { 88, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 72, -48, 1591, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 104, -48, 1557, { .fields = { 96, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 24 } }, -96, -72, 1609, { .fields = { 64, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 24 } }, -104, -96, 1572, { .fields = { 56, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 24 } }, -104, -120, 1503, { .fields = { 112, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 24 } }, -40, -96, 1540, { .fields = { 120, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 24 } }, -40, -120, 1506, { .fields = { 120, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 24 } }, -40, -72, 1596, { .fields = { 104, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 32 } }, -40, -48, 1628, { .fields = { 16, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 24 } }, -96, -48, 1688, { .fields = { 96, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, 80, -120, 1476, { .fields = { 104, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, 120, -120, 1420, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, 80, -96, 1502, { .fields = { 120, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 120, -96, 1460, { .fields = { 8, 160 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b2_operating_room_801829B4[14] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 16, 0, 0, { 7, 0 } },
    { 16, 11, 0, 0, { 2, 0 } },
    { 27, 13, 0, 0, { 6, 0 } },
    { 40, 15, 0, 0, { 1, 0 } },
    { 55, 9, 0, 0, { 9, 0 } },
    { 64, 1, 0, 0, { 0, 0 } },
    { 65, 6, 0, 0, { 10, 0 } },
    { 71, 13, 0, 0, { 5, 0 } },
    { 84, 2, 0, 0, { 8, 0 } },
    { 86, 2, 0, 0, { 4, 0 } },
    { 88, 18, 0, 0, { 11, 0 } },
    { 106, 18, 0, 0, { 3, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_shelter_b2_operating_room_80182A24[40] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 24, -80, 1041, { .fields = { 0, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 88, -24, 664, { .fields = { 0, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, 72, -104, 725, { .fields = { 104, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 80, -112, 675, { .fields = { 112, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, 56, -96, 766, { .fields = { 88, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, 40, -88, 837, { .fields = { 80, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, 32, -80, 923, { .fields = { 88, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 80, -120, 619, { .fields = { 0, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 72, -120, 669, { .fields = { 64, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 56, -120, 740, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 40, -120, 847, { .fields = { 0, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 24, -120, 983, { .fields = { 48, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 168 } }, 16, -120, 1120, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, 24, -16, 1056, { .fields = { 0, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, 40, -16, 915, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 96 } }, 56, -16, 824, { .fields = { 32, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 104 } }, 72, -16, 741, { .fields = { 48, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 104 } }, 96, -120, 640, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 120 } }, 88, -16, 658, { .fields = { 104, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 112 } }, 104, -120, 609, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 120 } }, 104, -8, 604, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 120 } }, 120, -120, 568, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 120 } }, 120, 0, 558, { .fields = { 64, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 120 } }, 136, -120, 521, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 120 } }, 136, 0, 510, { .fields = { 80, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, 72, -40, 730, { .fields = { 120, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 80, -40, 733, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 64, -32, 756, { .fields = { 72, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 40, -72, 1056, { .fields = { 32, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 24, -72, 1025, { .fields = { 0, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 24, 24, 792, { .fields = { 80, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 16, 40, 781, { .fields = { 120, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 32, 40, 794, { .fields = { 120, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 40, 24, 812, { .fields = { 32, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 24, 8, 810, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 40, 8, 873, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 88 } }, 88, -96, 692, { .fields = { 16, 88 } }, 128, 128, 128, 2 },
    { 143, 0x4000, { .fields = { 16, 88 } }, 72, -96, 766, { .fields = { 24, 0 } }, 128, 128, 128, 2 },
    { 143, 0x4000, { .fields = { 16, 80 } }, 56, -88, 858, { .fields = { 16, 176 } }, 128, 128, 128, 2 },
    { 143, 0x4000, { .fields = { 8, 72 } }, 48, -80, 944, { .fields = { 120, 168 } }, 128, 128, 128, 2 },
};

SpriteBatch D_shelter_b2_operating_room_80182D44[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 30, 0, 0, { 1, 0 } },
    { 30, 6, 0, 0, { 2, 0 } },
    { 36, 4, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_shelter_b2_operating_room_80182D6C[50] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -72, -120, 607, { .fields = { 72, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -88, -16, 625, { .fields = { 72, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -80, -16, 619, { .fields = { 72, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -72, -16, 632, { .fields = { 72, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, -136, -120, 604, { .fields = { 56, 8 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -96, -120, 655, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, -120, -112, 641, { .fields = { 64, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -112, -104, 654, { .fields = { 72, 16 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -88, -112, 673, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -88, -104, 681, { .fields = { 112, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 80 } }, -88, -96, 675, { .fields = { 112, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -120, -40, 506, { .fields = { 48, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 88 } }, -112, -96, 657, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 128 } }, -160, -8, 428, { .fields = { 104, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 128 } }, -136, -8, 494, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 128 } }, -112, -8, 575, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 120 } }, -88, -8, 645, { .fields = { 88, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 104 } }, -72, -8, 713, { .fields = { 48, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 112 } }, -64, -120, 766, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 96 } }, -64, -8, 775, { .fields = { 32, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, -48, -8, 816, { .fields = { 8, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -136, -16, 479, { .fields = { 88, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -160, -16, 463, { .fields = { 88, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -48, -64, 820, { .fields = { 24, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -48, -72, 838, { .fields = { 80, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -48, -120, 859, { .fields = { 96, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -40, -120, 897, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -40, -72, 880, { .fields = { 112, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -40, -64, 1003, { .fields = { 72, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -24, -120, 962, { .fields = { 40, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 88 } }, 0, -40, 1114, { .fields = { 24, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, 0, -120, 1096, { .fields = { 0, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 72 } }, 8, -112, 1165, { .fields = { 112, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, -16, -120, 1092, { .fields = { 8, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, -16, -40, 1114, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 104 } }, -40, -56, 1158, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -112, 48, 400, { .fields = { 88, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, -112, 72, 387, { .fields = { 80, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, -96, 88, 404, { .fields = { 80, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, -88, 104, 395, { .fields = { 88, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -64, 88, 395, { .fields = { 56, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, -152, 48, 362, { .fields = { 72, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -128, 72, 368, { .fields = { 96, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, -120, 88, 375, { .fields = { 88, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, -112, 104, 380, { .fields = { 88, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -120, 104, 470, { .fields = { 80, 240 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 112 } }, -160, -120, 487, { .fields = { 56, 112 } }, 128, 128, 128, 2 },
    { 143, 0x4000, { .fields = { 16, 112 } }, -144, -120, 522, { .fields = { 72, 128 } }, 128, 128, 128, 2 },
    { 143, 0x4000, { .fields = { 16, 112 } }, -128, -120, 563, { .fields = { 48, 0 } }, 128, 128, 128, 2 },
    { 142, 0x4000, { .fields = { 8, 72 } }, -112, -104, 596, { .fields = { 120, 88 } }, 128, 128, 128, 2 },
};

SpriteBatch D_shelter_b2_operating_room_80183154[6] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 26, 0, 0, { 3, 0 } },
    { 26, 10, 0, 0, { 0, 0 } },
    { 36, 10, 0, 0, { 2, 0 } },
    { 46, 4, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

GpSprtRec D_shelter_b2_operating_room_80183184[7] = {
    { { .empty = D_shelter_b2_operating_room_80181538 }, D_shelter_b2_operating_room_80181538, NULL },
    { { .elements = D_shelter_b2_operating_room_80181548 }, D_shelter_b2_operating_room_8018173C, NULL },
    { { .elements = D_shelter_b2_operating_room_8018175C }, D_shelter_b2_operating_room_801818B0, NULL },
    { { .elements = D_shelter_b2_operating_room_801818C8 }, D_shelter_b2_operating_room_80181F94, NULL },
    { { .elements = D_shelter_b2_operating_room_80182004 }, D_shelter_b2_operating_room_801829B4, NULL },
    { { .elements = D_shelter_b2_operating_room_80182A24 }, D_shelter_b2_operating_room_80182D44, NULL },
    { { .elements = D_shelter_b2_operating_room_80182D6C }, D_shelter_b2_operating_room_80183154, NULL },
};

GpPointLight D_shelter_b2_operating_room_801831D8[14] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 1560, -1747, 1797 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3276, 4014, 3932 }, { 0, 0 } }, 601, 1260 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x2800, -1615, 1827 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3440, 4096, 3686 }, { 0, 0 } }, 0, 2161 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 200, -1705, 4987 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2457, 3932, 3932 }, { 0, 0 } }, 500, 1300 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 540, -1731, 1487 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3440, 4096, 4096 }, { 0, 0 } }, 500, 2321 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 6454, 567, 2999 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3440, 4096, 4096 }, { 0, 0 } }, 1722, 2522 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 3997, 567, 2999 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3440, 4096, 4096 }, { 0, 0 } }, 1823, 2643 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x2800, -1534, 4217 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3440, 4096, 3686 }, { 0, 0 } }, 0, 2103 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 9030, -1847, 2448 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3276, 4014, 3932 }, { 0, 0 } }, 701, 1199 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x27C4, -1726, 5417 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3493, 3826, 4003 }, { 0, 0 } }, 681, 1481 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x2878, -1847, 548 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2736, 3442, 3506 }, { 0, 0 } }, 781, 1459 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 6730, -1579, 2920 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 409, 163 }, { 0, 0 } }, 0, 2582 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 3860, -1258, 2980 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 409, 163 }, { 0, 0 } }, 0, 3162 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 4541, -1560, 575 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 4096, 4096 }, { 0, 0 } }, 901, 1800 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 600, -1731, 3937 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3440, 4096, 4096 }, { 0, 0 } }, 500, 2021 },
};

GpRoomCoordSet D_shelter_b2_operating_room_80183718 = { 0, NULL, 14, D_shelter_b2_operating_room_801831D8, 0, NULL };

GpObj4C D_shelter_b2_operating_room_80183730[10] = {
    { NULL, NULL, NULL, { 2075, -1440, 1530, 0 }, { { 16, -1936, 1216, 0 }, { -16, -1936, -1215, 0 }, { 16, 1937, 1216, 0 }, { -16, 1937, -1215, 0 } }, { -4112, 0, 53, 0 }, { 0, 0, 4096, 0 }, 2275, 0, 7, 4, 1, 0 },
    { NULL, NULL, NULL, { 1916, -1408, 1628, 0 }, { { -16, -1936, -1312, 0 }, { 16, -1936, 1312, 0 }, { -16, 1937, -1312, 0 }, { 16, 1937, 1312, 0 } }, { 4115, 0, -52, 0 }, { 0, 0, 4096, 0 }, 2332, 0, 4, 7, 1, 0 },
    { NULL, NULL, NULL, { 8800, -1472, 2080, 0 }, { { 16, -1936, 1216, 0 }, { -16, -1936, -1215, 0 }, { 16, 1937, 1216, 0 }, { -16, 1937, -1215, 0 } }, { -4112, 0, 53, 0 }, { 0, 0, 4096, 0 }, 2275, 0, 5, 2, 1, 0 },
    { NULL, NULL, NULL, { 8673, -1440, 2112, 0 }, { { -16, -1936, -1312, 0 }, { 16, -1936, 1312, 0 }, { -16, 1937, -1312, 0 }, { 16, 1937, 1312, 0 } }, { 4115, 0, -52, 0 }, { 0, 0, 4096, 0 }, 2332, 0, 2, 5, 1, 0 },
    { NULL, NULL, NULL, { 5055, -1473, 3102, 0 }, { { 159, -1936, -3548, 0 }, { -158, -1936, 3548, 0 }, { 159, 1937, -3548, 0 }, { -158, 1937, 3548, 0 } }, { 4091, 0, 182, 0 }, { 0, 0, 4096, 0 }, 4039, 0, 5, 4, 1, 0 },
    { NULL, NULL, NULL, { 5280, -1505, 2960, 0 }, { { -152, -1936, 3370, 0 }, { 147, -1936, -3374, 0 }, { -152, 1937, 3370, 0 }, { 147, 1937, -3374, 0 } }, { -4097, 0, -182, 0 }, { 0, 0, 4096, 0 }, 3890, 0, 4, 5, 1, 0 },
    { NULL, NULL, NULL, { 464, -1473, 2736, 0 }, { { -1467, -1936, -130, 0 }, { 1465, -1936, 129, 0 }, { -1467, 1937, -130, 0 }, { 1465, 1937, 129, 0 } }, { 359, 0, -4084, 0 }, { 0, 0, 4096, 0 }, 2428, 0, 7, 6, 1, 0 },
    { NULL, NULL, NULL, { 512, -1441, 2625, 0 }, { { 1398, -1936, 152, 0 }, { -1402, -1936, -155, 0 }, { 1398, 1937, 152, 0 }, { -1402, 1937, -155, 0 } }, { -449, 0, 4075, 0 }, { 0, 0, 4096, 0 }, 2387, 0, 6, 7, 1, 0 },
    { NULL, NULL, NULL, { 0x28E1, -1504, 3072, 0 }, { { 1398, -1936, -127, 0 }, { -1407, -1936, 119, 0 }, { 1398, 1937, -127, 0 }, { -1407, 1937, 119, 0 } }, { 357, 0, 4082, 0 }, { 0, 0, 4096, 0 }, 2387, 0, 3, 2, 1, 0 },
    { NULL, NULL, NULL, { 0x28C0, -1408, 3200, 0 }, { { -1473, -1936, 84, 0 }, { 1465, -1936, -92, 0 }, { -1473, 1937, 84, 0 }, { 1465, 1937, -92, 0 } }, { -246, 0, -4093, 0 }, { 0, 0, 4096, 0 }, 2428, 0, 2, 3, 129, 0 },
};

GpObj3A D_shelter_b2_operating_room_80183A28[3] = {
    { NULL, NULL, { 2144, -1648, 3968, 0 }, { { 0, 2672, 1920, 0 }, { 0, 2672, -1920, 0 }, { 0, -2672, 1920, 0 }, { 0, -2672, -1920, 0 } }, { 4110, 0, 0, 0 }, { -40, 12 }, 1, 0 },
    { NULL, NULL, { 8576, -1504, 4640, 0 }, { { 0, 2528, 1840, 0 }, { 0, 2528, -1840, 0 }, { 0, -2528, 1840, 0 }, { 0, -2528, -1840, 0 } }, { 4113, 0, 0, 0 }, { 52, 12 }, 1, 0 },
    { NULL, NULL, { 8576, 0, 400, 0 }, { { 0, 2672, 864, 0 }, { 0, 2672, -864, 0 }, { 0, -2672, 864, 0 }, { 0, -2672, -864, 0 } }, { 4109, 0, 0, 0 }, { -12, 10 }, 129, 0 },
};

GpObj4C D_shelter_b2_operating_room_80183ADC[13] = {
    { NULL, NULL, NULL, { 0x2810, -48, 464, 0 }, { { -720, 0, -432, 0 }, { 720, 0, -432, 0 }, { -720, 0, 432, 0 }, { 720, 0, 432, 0 } }, { 0, 4110, 0, 0 }, { 0, 0, 4096, 0 }, 839, 0, 28, 18, 2, 0 },
    { NULL, NULL, NULL, { 0x2820, -48, 5568, 0 }, { { -864, 0, -432, 0 }, { 864, 0, -432, 0 }, { -864, 0, 432, 0 }, { 864, 0, 432, 0 } }, { 0, 4102, 0, 0 }, { 0, 0, -4096, 0 }, 964, 0, 30, 33, 2, 0 },
    { NULL, NULL, NULL, { 528, -48, 864, 0 }, { { -720, 0, -432, 0 }, { 720, 0, -432, 0 }, { -720, 0, 432, 0 }, { 720, 0, 432, 0 } }, { 0, 4110, 0, 0 }, { 0, 0, 4096, 0 }, 839, 0, 31, 50, 2, 0 },
    { NULL, NULL, NULL, { 7552, -64, 4288, 0 }, { { -416, 0, -688, 0 }, { 416, 0, -688, 0 }, { -416, 0, 688, 0 }, { 416, 0, 688, 0 } }, { 0, 4095, 0, 0 }, { -4096, 0, 0, 0 }, 801, 2, 1, 0, 2, 0 },
    { NULL, NULL, NULL, { 1376, -64, 2544, 0 }, { { -416, 0, -448, 0 }, { 416, 0, -448, 0 }, { -416, 0, 448, 0 }, { 416, 0, 448, 0 } }, { 0, 4102, 0, 0 }, { -4096, 0, 0, 0 }, 610, 2, 7, 0, 2, 0 },
    { NULL, NULL, NULL, { 4336, -64, 896, 0 }, { { -816, 0, -368, 0 }, { 816, 0, -368, 0 }, { -816, 0, 368, 0 }, { 816, 0, 368, 0 } }, { 0, 4115, 0, 0 }, { 0, 0, 4096, 0 }, 893, 2, 4, 255, 2, 0 },
    { NULL, NULL, NULL, { 5216, -64, 3040, 0 }, { { -1936, 0, -1456, 0 }, { 1936, 0, -1456, 0 }, { -1936, 0, 1456, 0 }, { 1936, 0, 1456, 0 } }, { 0, 4105, 0, 0 }, { 0, 0, 4096, 0 }, 2415, 2, 5, 255, 4, 0 },
    { NULL, NULL, NULL, { 2960, -64, 4592, 0 }, { { -784, 0, -416, 0 }, { 784, 0, -416, 0 }, { -784, 0, 416, 0 }, { 784, 0, 416, 0 } }, { 0, 4111, 0, 0 }, { 0, 0, -4096, 0 }, 886, 2, 6, 0, 2, 0 },
    { NULL, NULL, NULL, { -192, -64, 2912, 0 }, { { -400, 0, -1296, 0 }, { 400, 0, -1296, 0 }, { -400, 0, 1296, 0 }, { 400, 0, 1296, 0 } }, { 0, 4095, 0, 0 }, { 4096, 0, 0, 0 }, 1354, 2, 8, 0, 2, 0 },
    { NULL, NULL, NULL, { 320, -64, 4784, 0 }, { { -720, 0, -448, 0 }, { 720, 0, -448, 0 }, { -720, 0, 448, 0 }, { 720, 0, 448, 0 } }, { 0, 4114, 0, 0 }, { 0, 0, -4096, 0 }, 846, 2, 9, 0, 2, 0 },
    { NULL, NULL, NULL, { 3103, -64, 1175, 0 }, { { -738, 0, -192, 0 }, { 521, 0, -871, 0 }, { -744, 0, 952, 0 }, { 963, 0, 113, 0 } }, { 0, 4105, 0, 0 }, { 2896, 0, 2896, 0 }, 1207, 2, 10, 0, 2, 0 },
    { NULL, NULL, NULL, { 6928, -64, 4608, 0 }, { { -1024, 0, -448, 0 }, { 1024, 0, -448, 0 }, { -1024, 0, 448, 0 }, { 1024, 0, 448, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, -4096, 0 }, 1115, 2, 11, 0, 2, 0 },
    { NULL, NULL, NULL, { 7847, -64, 1343, 0 }, { { -763, 0, -1005, 0 }, { 787, 0, -200, 0 }, { -802, 0, 233, 0 }, { 780, 0, 974, 0 } }, { 0, 4098, 0, 0 }, { -1931, 0, 3612, 0 }, 1260, 2, 12, 0, 130, 0 },
};

GpAreaTmdRec D_shelter_b2_operating_room_80183EB8[5] = {
    { 70, 70, 0, 0, { 0, 0 }, D_8013F5F0 },
    { 71, 71, 0, 0, { 0, 0 }, D_80139E60 },
    { 72, 72, 1, 0, { 0, 0 }, D_80153EC8 },
    { 73, 73, 1, 0, { 0, 0 }, D_8014E7A4 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_shelter_b2_operating_room_80183EF4[2] = {
    { 24, 24, 0, 0, { 0, 0 }, D_8013647C },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_shelter_b2_operating_room_80183F0C[2] = {
    { 26, 26, 0, 0, { 0, 0 }, D_8013A8D4 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_shelter_b2_operating_room_80183F24[4] = {
    { 24, 24, 0, 0, { 0, 0 }, D_8013647C },
    { 70, 70, 1, 0, { 0, 0 }, &D_801575F0 },
    { 71, 71, 1, 0, { 0, 0 }, &D_80151E60 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

AreaPlacement D_shelter_b2_operating_room_80183F54[7] = {
    { 70, 0, 0, 5250, 0, 900, 800, 0, 0, 2, 0 },
    { 70, 0, 0, 0, 0, 2600, 2300, 0, 0, 2, 0 },
    { 72, 0, 0, 4900, 0, 4400, 1200, 0, 2, 4, 0 },
    { 72, 0, 0, 5500, 0, 1500, 1024, 0, 2, 4, 0 },
    { 72, 0, 0, 3700, 0, 3000, 2048, 0, 2, 4, 0 },
    { 73, 0, 1, 0x2710, 0, 3000, 3072, 0, 2, 4, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaPlacement D_shelter_b2_operating_room_80183FC4[5] = {
    { 24, 0, 0, 3100, 0, 4300, 1024, 0, 0, 2, 0 },
    { 24, 0, 0, 7200, 0, 4300, 2048, 0, 0, 2, 0 },
    { 24, 0, 0, 3100, 0, 1600, 0, 0, 0, 2, 0 },
    { 24, 0, 0, 7200, 0, 1600, 3072, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaPlacement D_shelter_b2_operating_room_80184014[8] = {
    { 26, 0, 1, 7800, 0, 1950, 1150, 0, 0, 2, 0 },
    { 26, 0, 1, 7900, 0, 2250, 950, 0, 0, 2, 0 },
    { 26, 0, 1, 500, 0, 4500, 1950, 0, 0, 2, 0 },
    { 26, 0, 0, 6700, 0, 4300, 1850, 0, 0, 2, 0 },
    { 26, 0, 0, 5400, 0, 1400, 500, 0, 0, 2, 0 },
    { 26, 0, 0, 3600, 0, 4100, 0, 0, 0, 2, 0 },
    { 26, 0, 0, 400, 0, 2900, 1700, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaPlacement D_shelter_b2_operating_room_80184094[9] = {
    { 24, 0, 0, 5700, 0, 1900, 800, 0, 0, 2, 0 },
    { 24, 0, 0, 3650, 0, 1500, 400, 0, 0, 2, 0 },
    { 24, 0, 0, 4950, 0, 4700, 2650, 0, 0, 2, 0 },
    { 24, 0, 0, 800, 0, 3400, 1950, 0, 0, 2, 0 },
    { 70, 0, 0, 5250, 0, 900, 800, 0, 2, 4, 0 },
    { 70, 0, 0, 0, 0, 2450, 1800, 0, 2, 4, 0 },
    { 70, 0, 0, 0x2904, 0, 3300, 2800, 0, 2, 4, 0 },
    { 71, 0, 1, 9200, 0, 2200, 1024, 0, 2, 4, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

GpAreaVariant D_shelter_b2_operating_room_80184124[12] = {
    { NULL, NULL },
    { D_shelter_b2_operating_room_80183F54, D_shelter_b2_operating_room_80183EB8 },
    { D_shelter_b2_operating_room_80183FC4, D_shelter_b2_operating_room_80183EF4 },
    { D_shelter_b2_operating_room_80184014, D_shelter_b2_operating_room_80183F0C },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { D_shelter_b2_operating_room_80184094, D_shelter_b2_operating_room_80183F24 },
};

WorldCoordRoomAmbientEntry D_shelter_b2_operating_room_80184184[8] = {
    { .viewCount = ARRAY_SIZE(D_shelter_b2_operating_room_80184184) - 1 },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 490, 520, 560, 513 } },
    { .color = { 400, 438, 400, 419 } },
    { .color = { 380, 758, 617, 598 } },
    { .color = { 641, 701, 691, 677 } },
    { .color = { 277, 257, 260, 264 } },
    { .color = { 620, 630, 690, 633 } },
};

s32 D_shelter_b2_operating_room_801841C4[3] = {
    0x10000009,
    0x1000000B,
    0x10000009,
};

s32 D_shelter_b2_operating_room_801841D0[3] = {
    0x1000005D,
    0x1000005F,
    0x1000005D,
};

GpRoomParamRec D_shelter_b2_operating_room_801841DC[1] = {
    { 0, 0, 1, 0, NULL },
};

GpRoomParamRec D_shelter_b2_operating_room_801841E4[1] = {
    { 0, 0, 1, 0, D_shelter_b2_operating_room_801841C4 },
};

GpRoomParamRec D_shelter_b2_operating_room_801841EC[1] = {
    { 0, 0, 1, 0, D_shelter_b2_operating_room_801841D0 },
};

GpRoomParamRec* D_shelter_b2_operating_room_801841F4[8] = {
    D_shelter_b2_operating_room_801841DC,
    D_shelter_b2_operating_room_801841E4,
    D_shelter_b2_operating_room_801841EC,
    D_shelter_b2_operating_room_801841DC,
    D_shelter_b2_operating_room_801841DC,
    D_shelter_b2_operating_room_801841DC,
    D_shelter_b2_operating_room_801841DC,
    D_shelter_b2_operating_room_801841DC,
};

RoomFadeStorage D_shelter_b2_operating_room_80184214 = { 0 };

RoomEventMsg D_shelter_b2_operating_room_8018421C = { 0 };

ShelterB2OperatingRoomStorage4224 D_shelter_b2_operating_room_80184224 = { 0 };

RoomEventMsg D_shelter_b2_operating_room_8018422C = { 0 };

u8 D_shelter_b2_operating_room_80184234[4] = {
    0,
    37,
    9,
    8,
};

ShelterB2OperatingRoomStorage4238 D_shelter_b2_operating_room_80184238;

RoomLatchedEvent D_shelter_b2_operating_room_80184258;

static s32            func_shelter_b2_operating_room_8017D628(RoomEventReq* req, RoomEventMsg* msg);
static __inline__ s32 _operatingRoomStartEvent(RoomEventMsg* dst, RoomLatchedEvent* event);
static void           func_shelter_b2_operating_room_8017DD14(Task* task);
static void           func_shelter_b2_operating_room_8017DD58(Task* task);

/// Handles a request to leave through a flag-gated exit. When the flag named
/// by `req->flagId` (negated: must be clear) is already in the wanted state,
/// returns 1. Otherwise, if the item `req->itemId` has been collected (or none
/// is needed), sets the flag, records `msg` and `req`, spawns the transition
/// task and returns 2; if the item is missing, runs cap command `req->field_4`
/// and returns 0. The spawn and the cap command happen only when
/// `msg->queryOnly` is 0.
static s32 func_shelter_b2_operating_room_8017D628(RoomEventReq* req, RoomEventMsg* msg)
{
    s32 flag;
    s32 id;
    s32 mode;
    s32 got;
    s32 ret;
    s32 neg;

    flag                                       = req->flagId;
    D_shelter_b2_operating_room_80184224.value = 0;
    neg                                        = flag < 0;
    got                                        = (s16)flag;
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
            if (msg->queryOnly == ROOM_EVENT_EXECUTE) {
                D_shelter_b2_operating_room_8018421C       = *msg;
                D_shelter_b2_operating_room_80184238.value = *req;
                id                                         = req->flagId;
                mode                                       = 1;
                if (id < 0) {
                    id   = -id;
                    mode = 0;
                }
                GameFlag_SetNibble(id, mode);
                Task_SpawnFromTable(&D_shelter_b2_operating_room_80180904, 0, 0, 0);
                D_shelter_b2_operating_room_80184224.value = 1;
                return 2;
            }
            return ret;
        }
        ret = 0;
        if (msg->queryOnly == ROOM_EVENT_EXECUTE) {
            Gp_RunCapCmd1(req->field_4);
            Gp_SetNibbleIf(msg->flagId, 2);
            ret = 0;
        }
        return ret;
    }
    return ret;
}

/// Transition task for an exit the gate accepted: runs the latched request's
/// cap command `field_0`, plays its sounds `field_8` and then `field_C` (each
/// only when non-zero), waiting for each voice to finish, then records the
/// latched message's area, warp and room in the save data's location, spawns
/// task 0x11 of bank 0 and kills itself.
void func_shelter_b2_operating_room_8017D78C(Task* task)
{
    switch (task->state) {
        case 0:
            Gp_StateF0.field_4 = 1;
            Gp_MsgPlayerWeapon(0);
            Gp_RunCapCmd1(D_shelter_b2_operating_room_80184238.value.field_0);
            if (D_shelter_b2_operating_room_80184238.value.field_8 != 0) {
                SndEvt_EnqueueType6(D_shelter_b2_operating_room_80184238.value.field_8, 0, 0);
                task->state++;
            } else {
                task->state = 2;
            }
            break;
        case 1:
            if (SndVoice_HasActiveId(D_shelter_b2_operating_room_80184238.value.field_8) == 0) {
                task->state++;
            }
            break;
        case 2:
            task->state++;
            break;
        case 3:
            if (D_shelter_b2_operating_room_80184238.value.field_C != 0) {
                SndEvt_EnqueueType6(D_shelter_b2_operating_room_80184238.value.field_C, 0, 0);
                task->state++;
            } else {
                task->state = 5;
            }
            break;
        case 4:
            if (SndVoice_HasActiveId(D_shelter_b2_operating_room_80184238.value.field_C) == 0) {
                task->state++;
            }
            break;
        case 5:
            SndEvt_EnqueueType7(0x80000000, 0);
            gDisplayState.spriteVariant       = 1;
            Mc_SaveData[0].state.at4.loc.area = D_shelter_b2_operating_room_8018421C.areaId;
            Mc_SaveData[0].state.at4.loc.warp = D_shelter_b2_operating_room_8018421C.warp;
            Mc_SaveData[0].state.at4.loc.room = (u8)D_shelter_b2_operating_room_8018421C.room;
            Task_Spawn(0, 0x11, 0, 0);
            taskKill(task);
            break;
    }
}

/// The room's own event task, spawned by its message handler. State 0 runs
/// the latched event's CAP command; state 1 waits for it to finish and, when
/// the event asks for it, starts helper task 0x31; states 2 and 3 play the
/// event's stage sound and wait for it; state 4 writes the latched message's
/// destination into the save data and hands over to task type 0x11.
void func_shelter_b2_operating_room_8017D8FC(Task* arg0)
{
    switch (arg0->state) {
        case 0:
            Gp_StateF0.field_4 = 1;
            Gp_MsgPlayerWeapon(0);
            Gp_RunCapCmd(D_shelter_b2_operating_room_80184258.capCmd, 0);
            D_80115690 = 1;
            arg0->state++;
            break;
        case 1:
            if (Gp_CapBusy() == 0) {
                if (D_shelter_b2_operating_room_80184258.fade != 0) {
                    D_shelter_b2_operating_room_80184214.fade.field_0 = 0;
                    D_shelter_b2_operating_room_80184214.fade.field_1 = 0;
                    D_shelter_b2_operating_room_80184214.fade.field_2 = 0x1E;
                    Task_Spawn(1, 0x31, 0, &D_shelter_b2_operating_room_80184214.fade);
                }
                arg0->state++;
            }
            break;
        case 2:
            if (D_shelter_b2_operating_room_80184258.stageSnd != 0) {
                Gp_EnqueueStageSnd6(D_shelter_b2_operating_room_80184258.stageSnd, 0, 0);
                arg0->state++;
            } else {
                arg0->state = 4;
            }
            break;
        case 3:
            if (SndVoice_HasActiveId(Gp_PackStageSndId(D_shelter_b2_operating_room_80184258.stageSnd)) == 0) {
                arg0->state++;
            }
            break;
        case 4:
            SndEvt_EnqueueType7(0x80000000, 0);
            gDisplayState.spriteVariant       = 1;
            Mc_SaveData[0].state.at4.loc.area = D_shelter_b2_operating_room_8018422C.areaId;
            Mc_SaveData[0].state.at4.loc.warp = D_shelter_b2_operating_room_8018422C.warp;
            Mc_SaveData[0].state.at4.loc.room = D_shelter_b2_operating_room_8018422C.room;
            Task_Spawn(0, 0x11, 0, 0);
            taskKill(arg0);
            break;
    }
}

/// Starts `event` for the outgoing message `dst` unless its flag says it has
/// already happened (answering 1). Otherwise answers 2, and - unless
/// `dst->queryOnly` asks for a dry run - latches the message and the event,
/// sets the flag and spawns the room's event task.
static __inline__ s32 _operatingRoomStartEvent(RoomEventMsg* dst, RoomLatchedEvent* event)
{
    D_shelter_b2_operating_room_80184234[0] = 0;
    if (GameFlag_GetNibble(event->flagId) == 0 || event->flagId == 0) {
        if (dst->queryOnly == ROOM_EVENT_EXECUTE) {
            D_shelter_b2_operating_room_8018422C = *dst;
            D_shelter_b2_operating_room_80184258 = *event;
            if (event->flagId != 0) {
                GameFlag_SetNibble(event->flagId, 1);
            }
            Task_SpawnFromTable(&D_shelter_b2_operating_room_80180910, 0, 0, 0);
            D_shelter_b2_operating_room_80184234[0] = 1;
        }
        return 2;
    }
    return 1;
}

/// Message handler: copies the incoming message to `out` and forwards both to
/// `func_map_shelter_80179A04`. Message 0x1E goes through the exit gate
/// `func_shelter_b2_operating_room_8017D628` on flag 0xA8. Message 0x1C, while nibble 0xAA is clear, answers 0 and - unless
/// `in->queryOnly` asks for a dry run - passes `in->flagId` to `Gp_SetNibbleIf`
/// and runs cap command 3; once the nibble is set it starts the room event on
/// flag 0x13A instead. Message 0x1F starts the event on flag 0x13B; any other
/// message answers 1.
s32 func_shelter_b2_operating_room_8017DA94(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    RoomEventReq     req;
    RoomLatchedEvent event;

    *out = *in;
    func_map_shelter_80179A04(in, out);
    if (in->areaId == 0x1E) {
        req.field_0 = 2;
        req.field_4 = 1;
        req.field_8 = 0x541D0007;
        req.field_C = 0x541D0003;
        req.flagId  = 0xA8;
        req.itemId  = 0;
        return func_shelter_b2_operating_room_8017D628(&req, out);
    }
    if (in->areaId == 0x1C && GameFlag_GetNibble(0xAA) == 0) {
        if (in->queryOnly == ROOM_EVENT_EXECUTE) {
            Gp_SetNibbleIf(in->flagId, 2);
            Gp_RunCapCmd1(3);
        }
        return 0;
    }
    if (in->areaId == 0x1C) {
        event.capCmd   = 0xE;
        event.stageSnd = 0x541D0001;
        event.flagId   = 0x13A;
        event.fade     = 0;
        return _operatingRoomStartEvent(out, &event);
    }
    if (in->areaId == 0x1F) {
        event.capCmd   = 0xD;
        event.stageSnd = 0x541D0005;
        event.flagId   = 0x13B;
        event.fade     = 0;
        return _operatingRoomStartEvent(out, &event);
    }
    return 1;
}

s32 func_shelter_b2_operating_room_8017DC9C(Task* task, s32 msgId, TaskMessageArg arg2, TaskMessageArg arg3)
{
    return 0;
}

s32 func_shelter_b2_operating_room_8017DCA4(Task* arg0, s32 arg1, s32 arg2, TaskMessageArg arg3)
{
    switch (arg2) {
        case 4:
            Gp_SpawnIfCapIdle(GameFlag_GetNibble(0xC7) == 0 ? 4 : 0x10, 0);
            break;
        case 5:
            Gp_SpawnIfCapIdle(GameFlag_GetNibble(0xC7) != 0 ? 0xF : 5, 0);
            break;
    }
    return 0;
}

s32 func_shelter_b2_operating_room_8017DD0C(Task* task, s32 msgId, TaskMessageArg arg2, TaskMessageArg arg3)
{
    return 0;
}

/// Installs `D_shelter_b2_operating_room_8018091C` as the task's message
/// table, registers the task in pointer slot 7 and steps it on one state.
static void func_shelter_b2_operating_room_8017DD14(Task* task)
{
    task->msgTable = D_shelter_b2_operating_room_8018091C;
    Game_SetPtrSlot(task, 7);
    task->state = (s32)(task->state + 1);
}

/// The room task's idle state, which does nothing.
static void func_shelter_b2_operating_room_8017DD58(Task* task)
{
}

/// The room task's three states, dispatched by
/// `func_shelter_b2_operating_room_8017DD60`: install the message table, idle,
/// end.
static const TaskFuncTable3 D_shelter_b2_operating_room_8017D5F0 = {
    { func_shelter_b2_operating_room_8017DD14, func_shelter_b2_operating_room_8017DD58, taskKill }
};

/// Runs the handler for the task's current state, from a local copy of
/// `D_shelter_b2_operating_room_8017D5F0`.
void func_shelter_b2_operating_room_8017DD60(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_shelter_b2_operating_room_8017D5F0;
    sp.funcs[task->state](task);
}

/// Per-frame view task. On its first frame it stores three ids in the gameplay
/// words `D_80115734`, `D_80115730` (the effect the halo task spawns) and
/// `D_80115754`; every frame it
/// draws the glows of the current view (views 2 to 7) at that view's points,
/// as capsules through `func_shelter_b2_operating_room_8017E118` and discs
/// through `func_shelter_b2_operating_room_8017E95C`.
void func_shelter_b2_operating_room_8017DDB8(Task* arg0)
{
    if (arg0->state == 0) {
        D_80115734  = 0x60222;
        D_80115730  = 0x6022D;
        D_80115754  = 0x60238;
        arg0->state = 1;
    }

    switch (Gp_GetViewIndex() & 0xFF) {
        case 2:
            func_shelter_b2_operating_room_8017E118(&D_shelter_b2_operating_room_80180B6C[0], 0x100, 0x444);
            func_shelter_b2_operating_room_8017E118(&D_shelter_b2_operating_room_80180B6C[4], 0x100, 0x444);
            func_shelter_b2_operating_room_8017E118(&D_shelter_b2_operating_room_80180B6C[8], 0x100, 0x444);
            break;
        case 3:
            func_shelter_b2_operating_room_8017E118(&D_shelter_b2_operating_room_80180B5C[0], 0x100, 0x444);
            func_shelter_b2_operating_room_8017E118(&D_shelter_b2_operating_room_80180B5C[8], 0x100, 0x444);
            break;
        case 4:
            func_shelter_b2_operating_room_8017E95C(&D_shelter_b2_operating_room_80180B44[0], 0x380, 0x444);
            func_shelter_b2_operating_room_8017E95C(&D_shelter_b2_operating_room_80180B44[-48], 0x200, 0x433);
            func_shelter_b2_operating_room_8017E95C(&D_shelter_b2_operating_room_80180B44[-44], 0x200, 0x433);
            func_shelter_b2_operating_room_8017E95C(&D_shelter_b2_operating_room_80180B44[-41], 0x200, 0x433);
            func_shelter_b2_operating_room_8017E95C(&D_shelter_b2_operating_room_80180B44[-38], 0x200, 0x433);
            func_shelter_b2_operating_room_8017E95C(&D_shelter_b2_operating_room_80180B44[-37], 0x200, 0x433);
            func_shelter_b2_operating_room_8017E95C(&D_shelter_b2_operating_room_80180B44[-36], 0x200, 0x433);
            func_shelter_b2_operating_room_8017E95C(&D_shelter_b2_operating_room_80180B44[-35], 0x200, 0x433);
            func_shelter_b2_operating_room_8017E95C(&D_shelter_b2_operating_room_80180B44[-34], 0x200, 0x433);
            func_shelter_b2_operating_room_8017E95C(&D_shelter_b2_operating_room_80180B44[-31], 0x200, 0x400);
            func_shelter_b2_operating_room_8017E95C(&D_shelter_b2_operating_room_80180B44[-30], 0x200, 0x400);
            func_shelter_b2_operating_room_8017E95C(&D_shelter_b2_operating_room_80180B44[-27], 0x200, 0x400);
            func_shelter_b2_operating_room_8017E95C(&D_shelter_b2_operating_room_80180B44[-26], 0x200, 0x400);
            func_shelter_b2_operating_room_8017E95C(&D_shelter_b2_operating_room_80180B44[-23], 0x200, 0x400);
            func_shelter_b2_operating_room_8017E95C(&D_shelter_b2_operating_room_80180B44[-22], 0x200, 0x400);
            func_shelter_b2_operating_room_8017E95C(&D_shelter_b2_operating_room_80180B44[-21], 0x200, 0x400);
            func_shelter_b2_operating_room_8017E118(&D_shelter_b2_operating_room_80180B44[-7], 0x100, 0x400);
            break;
        case 5:
            func_shelter_b2_operating_room_8017E95C(&D_shelter_b2_operating_room_801809BC[0], 0x200, 0x433);
            func_shelter_b2_operating_room_8017E95C(&D_shelter_b2_operating_room_801809BC[6], 0x200, 0x433);
            func_shelter_b2_operating_room_8017E95C(&D_shelter_b2_operating_room_801809BC[8], 0x200, 0x433);
            func_shelter_b2_operating_room_8017E95C(&D_shelter_b2_operating_room_801809BC[9], 0x200, 0x433);
            func_shelter_b2_operating_room_8017E95C(&D_shelter_b2_operating_room_801809BC[10], 0x200, 0x433);
            func_shelter_b2_operating_room_8017E95C(&D_shelter_b2_operating_room_801809BC[13], 0x200, 0x433);
            func_shelter_b2_operating_room_8017E95C(&D_shelter_b2_operating_room_801809BC[14], 0x200, 0x433);
            func_shelter_b2_operating_room_8017E95C(&D_shelter_b2_operating_room_801809BC[15], 0x200, 0x433);
            func_shelter_b2_operating_room_8017E95C(&D_shelter_b2_operating_room_801809BC[16], 0x200, 0x433);
            func_shelter_b2_operating_room_8017E95C(&D_shelter_b2_operating_room_801809BC[17], 0x200, 0x433);
            func_shelter_b2_operating_room_8017E95C(&D_shelter_b2_operating_room_801809BC[20], 0x200, 0x400);
            func_shelter_b2_operating_room_8017E95C(&D_shelter_b2_operating_room_801809BC[21], 0x200, 0x400);
            func_shelter_b2_operating_room_8017E95C(&D_shelter_b2_operating_room_801809BC[24], 0x200, 0x400);
            func_shelter_b2_operating_room_8017E95C(&D_shelter_b2_operating_room_801809BC[25], 0x200, 0x400);
            func_shelter_b2_operating_room_8017E95C(&D_shelter_b2_operating_room_801809BC[29], 0x200, 0x400);
            func_shelter_b2_operating_room_8017E95C(&D_shelter_b2_operating_room_801809BC[30], 0x200, 0x400);
            func_shelter_b2_operating_room_8017E95C(&D_shelter_b2_operating_room_801809BC[31], 0x200, 0x400);
            break;
        case 6:
            func_shelter_b2_operating_room_8017E118(&D_shelter_b2_operating_room_80180ABC[0], 0x100, 0x444);
            func_shelter_b2_operating_room_8017E118(&D_shelter_b2_operating_room_80180ABC[2], 0x100, 0x444);
            break;
        case 7:
            func_shelter_b2_operating_room_8017E118(&D_shelter_b2_operating_room_80180ADC[0], 0x100, 0x444);
            func_shelter_b2_operating_room_8017E118(&D_shelter_b2_operating_room_80180ADC[2], 0x100, 0x444);
            func_shelter_b2_operating_room_8017E118(&D_shelter_b2_operating_room_80180ADC[4], 0x100, 0x444);
            func_shelter_b2_operating_room_8017E118(&D_shelter_b2_operating_room_80180ADC[8], 0x100, 0x444);
            break;
    }
}

/// Draws a glowing capsule between the points `arg0[0]` and `arg0[1]`,
/// projected through `gGfxViewCoord.workm`; nothing is drawn unless both project.
/// Each end is a half-disc of screen radius `arg1 * 64 / otz` and the two are
/// joined by a band, built from gouraud quads lit at the centre line and black
/// at the rim, in two 0x400 steps around the angle between the projected
/// points. `arg2` is the colour as three 4-bit channels (0xRGB), brightened
/// slightly on odd frames.
static void func_shelter_b2_operating_room_8017E118(SVECTOR* arg0, s32 arg1, s32 arg2)
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
    scratch  = SCRATCH_STACK_CURSOR_SLOT;
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
                    prim           = gGpuPrimCursor;
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
                    addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz0 << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                            prim);
                    Gp_AddTpageShift((P_TAG*)prim, 1, block->otz0);

                    conn           = angStart + ((ang - angStart) * 2);
                    prim           = gGpuPrimCursor;
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
                    addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)((block->otz1 + block->otz0) / 2) << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                            prim);
                    Gp_AddTpageShift((P_TAG*)prim, 1, (block->otz1 + block->otz0) / 2);
                    t3             = ang + 0x800;
                    prim           = gGpuPrimCursor;
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
                    addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz1 << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                            prim);
                    Gp_AddTpageShift((P_TAG*)prim, 1, block->otz1);
                    ang = t2;
                } while (ang < limit);
            }
        }
    }
    SCRATCH_POP_BYTES(0x1C);
}

/// Draws a glowing disc around the point `arg0`, projected through
/// `gGfxViewCoord.workm`, unless the projection flags an error: four gouraud
/// wedges lit at the projected centre and black at the rim, of screen radius
/// `arg1 * 64 / otz`. `arg2` is the colour as three 4-bit channels (0xRGB),
/// brightened slightly on odd frames.
static void func_shelter_b2_operating_room_8017E95C(SVECTOR* arg0, s32 arg1, s32 arg2)
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
            prim           = gGpuPrimCursor;
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
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        } while (ang < 0x1000);
    }
    SCRATCH_STACK_RELEASE_BLOCK(RoomDraw13Scratch);
}

/// Halo effect task attached to a parent coordinate. State 0 places it at the
/// work block's position; states 1 and 2 grow a glowing disc tinted by the
/// channel shifts its spawn argument selects, state 1 also spawning effect
/// `D_80115730` on a random bone of the player every fourth tick and state 2
/// adding a fainter, wider disc on odd ticks. State 3 drifts away while
/// drawing a widening ring and fading, and releases the work block once faded;
/// state 4 releases it at once. While `Gp_State1C->effectControl` is non-zero it
/// does nothing but release once that reaches 4.
void func_shelter_b2_operating_room_8017ECFC(Task* arg0)
{
    GpEffWork* mem;
    GfxCoord*  coord;
    GpEffWork* spawned;
    MATRIX*    mtx;
    u8         col[4];

    mem   = arg0->spawnArg2.pointer;
    coord = arg0->extra.coordBody->coord;
    if (Gp_State1C->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
        if (Gp_State1C->effectControl < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
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
            col[0] = mem->scale >> D_shelter_b2_operating_room_80180BBC[arg0->spawnArg1.value][0];
            col[1] = mem->scale >> D_shelter_b2_operating_room_80180BBC[arg0->spawnArg1.value][1];
            col[2] = mem->scale >> D_shelter_b2_operating_room_80180BBC[arg0->spawnArg1.value][2];
            func_shelter_b2_operating_room_8017FB20(coord, mem->angle, col);
            break;
        case 2:
            Gp_UpdateCoord(coord);
            if (mem->scale < 0xC0) {
                mem->scale += 8;
            }
            if (mem->angle < 0x200) {
                mem->angle += 0x10;
            }
            col[0] = mem->scale >> D_shelter_b2_operating_room_80180BBC[arg0->spawnArg1.value][0];
            col[1] = mem->scale >> D_shelter_b2_operating_room_80180BBC[arg0->spawnArg1.value][1];
            col[2] = mem->scale >> D_shelter_b2_operating_room_80180BBC[arg0->spawnArg1.value][2];
            func_shelter_b2_operating_room_8017FB20(coord, mem->angle, col);
            col[0] >>= 1;
            col[1] >>= 1;
            col[2] >>= 1;
            if (mem->age & 1) {
                func_shelter_b2_operating_room_8017FB20(coord, (s16)(mem->angle + 0x100), col);
            }
            break;
        case 3:
            Gp_UpdateCoord(coord);
            col[0] = mem->scale >> D_shelter_b2_operating_room_80180BBC[arg0->spawnArg1.value][0];
            col[1] = mem->scale >> D_shelter_b2_operating_room_80180BBC[arg0->spawnArg1.value][1];
            col[2] = mem->scale >> D_shelter_b2_operating_room_80180BBC[arg0->spawnArg1.value][2];
            func_shelter_b2_operating_room_8017FB20(coord, mem->angle, col);
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
            func_shelter_b2_operating_room_8017F6FC(coord, (s16)(mem->period + 0x80), 0x100, col);
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

/// Effect task that drifts toward the coordinate in `spawnArg1`. Its first
/// frame turns the world-space offset to that coordinate into the effect's
/// own frame and keeps 0xCC/0x1000 of it as the per-frame step. Every frame
/// after that moves by the step and, on odd ticks, draws the next frame of
/// `func_shelter_b2_operating_room_8017F478`, releasing the effect at tick 20.
/// While `Gp_State1C->effectControl` is non-zero it does nothing but release from
/// state 4.
void func_shelter_b2_operating_room_8017F254(Task* task)
{
    GpEffWork* work;
    GfxCoord*  coord;
    GfxCoord*  target;
    VECTOR     delta;

    work   = task->spawnArg2.pointer;
    coord  = task->extra.coordBody->coord;
    target = task->spawnArg1.pointer;
    if (Gp_State1C->effectControl == ROOM_EFFECT_CONTROL_RUNNING) {
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
                    func_shelter_b2_operating_room_8017F478(coord, ++work->index, 0x200, 0x80);
                }
                if (work->age >= 20) {
                    Gp_ReleaseState1CMem(work, task);
                }
                break;
        }
    } else if (Gp_State1C->effectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN) {
        Gp_ReleaseState1CMem(work, task);
    }
}

/// Queues a semi-transparent textured square centred on the projected world
/// position of `arg0`, of half-size `arg2` scaled by depth. `arg1 & 3` picks
/// the animation frame from a row of four 24-texel frames and `arg3` is the
/// grey level. Nothing is drawn when the projection overflows.
static void func_shelter_b2_operating_room_8017F478(GfxCoord* arg0, s32 arg1, s32 arg2, s32 arg3)
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
    scratch                                 = SCRATCH_STACK_CURSOR_SLOT;
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

/// Draws a glowing ring around the coordinate's projected position, unless the
/// projection flags an error: sixteen gouraud quads, black at screen radius
/// `arg1 * 64 / (otz + 1)` and coloured `rgb` at
/// `(arg1 + arg2) * 64 / (otz + 1)`.
static void func_shelter_b2_operating_room_8017F6FC(GfxCoord* arg0, s32 arg1, s32 arg2, u8* rgb)
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
    SCRATCH_STACK_RELEASE_BLOCK(GpArcScratch);
}

/// Draws a glowing disc at the coordinate's world position, unless the
/// projection flags an error: eight gouraud wedges coloured `rgb` at the
/// projected centre and black at the rim, of screen radius
/// `arg1 * 64 / (otz + 1)`.
static void func_shelter_b2_operating_room_8017FB20(GfxCoord* arg0, s32 arg1, u8* rgb)
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
    SCRATCH_STACK_RELEASE_BLOCK(GpRingScratch);
}

/// Burst effect task. Every frame it draws a glowing disc and the glow of
/// `func_shelter_b2_operating_room_80180060` at a growing size. While its echo
/// level lasts it also draws a widening ring that fades out; once the echo is
/// spent the main level runs down, and the work block is released when it
/// does. While `Gp_State1C->effectControl` is non-zero it does nothing but
/// release once that reaches 4.
void func_shelter_b2_operating_room_8017FEB4(Task* arg0)
{
    u8         rgb[3];
    GpEffWork* mem;
    GfxCoord*  coord;
    s16        flag;
    s16        step;

    mem   = arg0->spawnArg2.pointer;
    flag  = Gp_State1C->effectControl;
    coord = arg0->extra.coordBody->coord;
    if (flag != ROOM_EFFECT_CONTROL_RUNNING) {
        if (flag < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
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
        func_shelter_b2_operating_room_8017FB20(coord, (s16)(step * 2), rgb);
        func_shelter_b2_operating_room_80180060(coord, mem->angle);
        if (mem->period >= 0x19) {
            rgb[0] = mem->period;
            rgb[1] = mem->period >> 1;
            rgb[2] = mem->period >> 2;
            func_shelter_b2_operating_room_8017F6FC(coord, (s16)(mem->step * 3 / 2), 0x60, rgb);
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
static void func_shelter_b2_operating_room_80180060(GfxCoord* coord, s16 size)
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

    slot                                          = &Gp_RoomCoords[2];
    slot->framesLeft                              = 2;
    light                                         = &slot->light;
    light->inner                                  = 0x300;
    light->outer                                  = 0x3000;
    random                                        = (Gp_LcgState * 5) + 0x71357911;
    intensity                                     = ((random >> 0x10) & 0x700) + 0x800;
    light->head.color.r                           = intensity;
    shifted                                       = intensity << 0x10;
    light->head.color.g                           = shifted >> 0x11;
    light->head.color.b                           = shifted >> 0x12;
    light->head.transform.lighting.local.t[0]     = coord->coord.t[0];
    light->head.transform.lighting.local.t[1]     = coord->coord.t[1];
    light->head.transform.lighting.local.t[2]     = coord->coord.t[2];
    slot->light.head.transform.coord.composeStamp = GRAPHICS_COORD_DIRTY;
    Gp_LcgState                                   = random;
    block                                         = SCRATCH_PUSH(GpRingScratch);
    block->vec.vx                                 = coord->workm.t[0];
    block->vec.vy                                 = coord->workm.t[1];
    block->vec.vz                                 = coord->workm.t[2];
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
            func_shelter_b2_operating_room_8018058C(&ground, outerSize);
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(GpRingScratch);
}

/// Queues one semi-transparent textured quad lying flat at the coordinate's
/// world position: the unit quad `D_80111E38` is scaled by `arg1`, turned by
/// the view matrix and projected through `GsWSMATRIX`. Unless the GTE flags
/// the projection, the quad is coloured (0x30, 0x20, 0x20) and its texture
/// alternates between two 32-pixel columns on successive frames.
static void func_shelter_b2_operating_room_8018058C(GfxCoord* arg0, s32 arg1)
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

    scratch  = SCRATCH_STACK_CURSOR_SLOT;
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
