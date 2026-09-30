#include "rooms/shelter_b1_main_corridor.h"

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
#include "gameplay/hud_sprites.h"
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
#include "main/gfx_types.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "mapui/map_shelter.h"

#include "rooms/room.h"

#include "rooms/room_common.h"

#include "rooms/rooms_shared_8017dcb8.h"

extern SVECTOR D_shelter_b1_main_corridor_801831E8[2];

// Preserve the following nonzero bytes with this scalar's storage.
// No separate references identify them; their role (including padding) is unresolved.
extern u8 D_shelter_b1_main_corridor_80185D44[4];

// Preserve the nonzero halfword after the three effect records.
// Its role is unresolved; it may be retained exporter padding.
typedef struct {
    RoomHaloShade entries[3];
    u16           retained;
} ShelterB1MainCorridorHaloStorage;
STATIC_ASSERT_SIZEOF(ShelterB1MainCorridorHaloStorage, 20);
extern ShelterB1MainCorridorHaloStorage D_shelter_b1_main_corridor_801831D4;

/// The event the gate last accepted: the message that triggered it, whose
/// `msgId`, `field_2` and `field_3` name the area, warp and room the event
/// task finally loads, and the request whose CAP command and sounds it runs.
extern RoomEventMsg D_shelter_b1_main_corridor_80185D2C;
// Only the leading value has established accesses. Preserve the following
// zero bytes in this allocation; trailing fields versus TU padding remains
// unresolved (see the local actors/rooms data review).
typedef struct {
    RoomEventReq value;
    u8           retained[12];
} ShelterB1MainCorridorStorage5D48;
STATIC_ASSERT_SIZEOF(ShelterB1MainCorridorStorage5D48, 32);

extern ShelterB1MainCorridorStorage5D48 D_shelter_b1_main_corridor_80185D48;
/// Set once the gate has latched an event and spawned its task.
// Only the leading value has established accesses. Preserve the following
// zero bytes in this allocation; trailing fields versus TU padding remains
// unresolved (see the local actors/rooms data review).
typedef struct {
    u8 value;
    u8 retained[7];
} ShelterB1MainCorridorStorage5D34;
STATIC_ASSERT_SIZEOF(ShelterB1MainCorridorStorage5D34, 8);

extern ShelterB1MainCorridorStorage5D34 D_shelter_b1_main_corridor_80185D34;
/// Spawn descriptor of the event task, `func_shelter_b1_main_corridor_8017D784`.
extern TaskDesc D_shelter_b1_main_corridor_8018308C;

/// The event the message handler last started: the message that triggered it,
/// the flag saying one was latched and its task spawned, and the event record.
/// `D_shelter_b1_main_corridor_80183098` spawns that event's task,
/// `func_shelter_b1_main_corridor_8017D8F4`.
extern RoomEventMsg     D_shelter_b1_main_corridor_80185D3C;
extern RoomLatchedEvent D_shelter_b1_main_corridor_80185D68;
extern TaskDesc         D_shelter_b1_main_corridor_80183098;
/// Spawn argument the event task hands to task 0x31.
extern RoomFadeStorage D_shelter_b1_main_corridor_80185D24;

/// The room's message table, which its tasks answer from.
extern GpMsgEntry D_shelter_b1_main_corridor_801830A4[];

/// Points the per-view drawing places its capsules and sprites at.
extern SVECTOR D_shelter_b1_main_corridor_801830D4[];
extern SVECTOR D_shelter_b1_main_corridor_80183114[];
extern SVECTOR D_shelter_b1_main_corridor_80183124[];
extern SVECTOR D_shelter_b1_main_corridor_80183134[];
extern SVECTOR D_shelter_b1_main_corridor_80183144[];

/// Per-colour shift amounts the flash effect's brightness is scaled down by,
/// one row per colour its spawner can pick.

/// The two points the beam runs between, relative to its parent coordinate;
/// the second is also declared on its own.

static void func_shelter_b1_main_corridor_8017E070(SVECTOR* arg0, s32 arg1, s32 arg2, s32 arg3);
static void func_shelter_b1_main_corridor_8017E858(SVECTOR* arg0, s32 arg1, s32 arg2);
static void func_shelter_b1_main_corridor_8017EDA0(GfxCoord* arg0, u16 arg1, u16 arg2, u16 arg3);
static void func_shelter_b1_main_corridor_8017FD60(GfxCoord* coord, s16 size);
static void func_shelter_b1_main_corridor_8018028C(GfxCoord* arg0, s32 arg1);
static void func_shelter_b1_main_corridor_80180604(GfxCoord* arg0, s16 arg1, u8* arg2);
static void func_shelter_b1_main_corridor_8018139C(GfxCoord* arg0, s32 arg1, s32 arg2, u8* rgb);
static void func_shelter_b1_main_corridor_801817C8(GfxCoord* arg0, s16 arg1, u8* arg2);
static void func_shelter_b1_main_corridor_8018204C(GfxCoord* arg0, GfxCoord* arg1, s16 arg2, s16 arg3);
static void func_shelter_b1_main_corridor_801826CC(GfxCoord* arg0, s16 arg1, u8* arg2);

void func_shelter_b1_main_corridor_8017D784(Task*);
void func_shelter_b1_main_corridor_8017D8F4(Task*);
s32  func_shelter_b1_main_corridor_8017DA8C(Task*, s32, RoomEventMsg*, RoomEventMsg*);
s32  func_shelter_b1_main_corridor_8017DCEC(Task*, s32, GpMessageArg, GpMessageArg);
s32  func_shelter_b1_main_corridor_8017DCF4(Task*, s32, GpMessageArg, GpMessageArg);
s32  func_shelter_b1_main_corridor_8017DCFC(Task*, s32, GpMessageArg, GpMessageArg);
s32  func_shelter_b1_main_corridor_8017DD04(Task*, s32, s32, s32);

TaskDesc D_shelter_b1_main_corridor_8018308C = { 0, 32, func_shelter_b1_main_corridor_8017D784, { .model = NULL } };

TaskDesc D_shelter_b1_main_corridor_80183098 = { 0, 32, func_shelter_b1_main_corridor_8017D8F4, { .model = NULL } };

GpMsgEntry D_shelter_b1_main_corridor_801830A4[6] = {
    { 5102, func_shelter_b1_main_corridor_8017DA8C },
    { 5105, func_shelter_b1_main_corridor_8017DCEC },
    { 5103, func_shelter_b1_main_corridor_8017DCFC },
    { 5104, func_shelter_b1_main_corridor_8017DCF4 },
    { 5106, func_shelter_b1_main_corridor_8017DD04 },
    { 0x7FFFFFFF, NULL },
};

SVECTOR D_shelter_b1_main_corridor_801830D4[8] = {
    { -667, -1910, -0x4A3D, 0 },
    { -667, -1910, -0x45ED, 0 },
    { -667, -1910, -0x4309, 0 },
    { -667, -1910, -0x3EB4, 0 },
    { 668, -1910, -0x4A3D, 0 },
    { 668, -1910, -0x45ED, 0 },
    { 668, -1910, -0x4309, 0 },
    { 668, -1910, -0x3EB4, 0 },
};

SVECTOR D_shelter_b1_main_corridor_80183114[2] = {
    { -3246, -2265, -0x2988, 0 },
    { -3246, -2265, -9370, 0 },
};

SVECTOR D_shelter_b1_main_corridor_80183124[2] = {
    { -3246, -2265, -2620, 0 },
    { -3246, -2265, -1381, 0 },
};

SVECTOR D_shelter_b1_main_corridor_80183134[2] = {
    { 3260, -2265, -0x2988, 0 },
    { 3260, -2265, -9370, 0 },
};

SVECTOR D_shelter_b1_main_corridor_80183144[18] = {
    { 3260, -2265, -2620, 0 },
    { 3260, -2265, -1381, 0 },
    { -365, -2226, -180, 0 },
    { 365, -2226, -180, 0 },
    { -1293, -4768, -0x34A6, 0 },
    { -1293, -4768, -0x2CE0, 0 },
    { -1293, -4768, -9483, 0 },
    { -1293, -4768, -7483, 0 },
    { -1293, -4768, -5483, 0 },
    { -1293, -4768, -3483, 0 },
    { -1293, -4768, -1483, 0 },
    { 1293, -4768, -0x34A6, 0 },
    { 1293, -4768, -0x2CE0, 0 },
    { 1293, -4768, -9483, 0 },
    { 1293, -4768, -7483, 0 },
    { 1293, -4768, -5483, 0 },
    { 1293, -4768, -3483, 0 },
    { 1293, -4768, -1483, 0 },
};

ShelterB1MainCorridorHaloStorage D_shelter_b1_main_corridor_801831D4 = { { { 0, 1, 2 }, { 2, 1, 0 }, { 0, 2, 1 } }, 0x70C2 };

// The following record is dereferenced through an indexed view of this base; keep the complete bounded pool.
SVECTOR D_shelter_b1_main_corridor_801831E8[2] = {
    { 0, 190, -15, 0 },
    { 0, 1085, 180, 0 },
};

u8* D_shelter_b1_main_corridor_801831F8[1] = {
    D_8010CAF8,
};

GpViewCountRec D_shelter_b1_main_corridor_801831FC[1] = {
    { { .bytes = { 10, 0 } } },
};

GpWarpRec D_shelter_b1_main_corridor_80183200[6] = {
    { { .words = { 0, 0, 0, -0x4A38 } }, { 0, 0, 0, 0 }, { .words = { 0, 10, 0, -390 } }, { 0, 0, 0, 0 }, 0x540F0002, 0x540F0001, 0, 2, 0, 461 },
    { { .words = { 1024, -3467, 0, -0x2745 } }, { 0, 0, 0, 0 }, { .words = { 0, 10, 0, -390 } }, { 0, 0, 0, 0 }, 0x540F0002, 0x540F0001, 0, 6, 0, 0 },
    { { .words = { 3072, 3510, 0, -0x2729 } }, { 0, 0, 0, 0 }, { .words = { 0, 10, 0, -390 } }, { 0, 0, 0, 0 }, 0x540F0002, 0x540F0001, 0, 5, 0, 0 },
    { { .words = { 1024, -3500, 0, -1940 } }, { 0, 0, 0, 0 }, { .words = { 1024, -3500, 0, -1940 } }, { 0, 0, 0, 0 }, 0x540F0002, 0x540F0001, 0x540F000D, 10, 0, 454 },
    { { .words = { 3072, 3500, 0, -1940 } }, { 0, 0, 0, 0 }, { .words = { 0, 10, 0, -390 } }, { 0, 0, 0, 0 }, 0x540F0002, 0x540F0001, 0, 9, 0, 0 },
    { { .words = { 2048, 0, 0, 0 } }, { 0, 0, 0, 0 }, { .words = { 0, 10, 0, -390 } }, { 0, 0, 0, 0 }, 0x540F0002, 0x540F0001, 0, 8, 0, 0 },
};

SVECTOR D_shelter_b1_main_corridor_80183350[34] = {
#include "assets/shelter_b1_main_corridor_collision_06B30_normals.inc"
};

SVECTOR D_shelter_b1_main_corridor_80183460[172] = {
#include "assets/shelter_b1_main_corridor_collision_06B30_verts.inc"
};

GpGridFace D_shelter_b1_main_corridor_801839C0[82] = {
#include "assets/shelter_b1_main_corridor_collision_06B30_faces.inc"
};

s16 D_shelter_b1_main_corridor_80183D98[392] = {
#include "assets/shelter_b1_main_corridor_collision_06B30_cells.inc"
};

#define GRID_CELL(i) (&D_shelter_b1_main_corridor_80183D98[i])
s16* D_shelter_b1_main_corridor_801840A8[18] = {
#include "assets/shelter_b1_main_corridor_collision_06B30_table.inc"
};
#undef GRID_CELL

GpGridParams D_shelter_b1_main_corridor_801840F0 = { NULL, D_shelter_b1_main_corridor_80183350, D_shelter_b1_main_corridor_80183460, D_shelter_b1_main_corridor_801839C0, D_shelter_b1_main_corridor_801840A8, 5398, 0x4D03, 3, 6, 4000, 82 };

GpViewRec D_shelter_b1_main_corridor_80184114[10] = {
    { { { { 4095, 0, 0 }, { 0, 0, -4096 }, { 0, 4095, 0 } }, { 0, 0x5EE9, 8000 } }, 230 },
    { { { { -4024, 0, 762 }, { 150, 4015, 794 }, { -747, 808, -3945 } }, { -721, 1939, 0x2F2F } }, 275 },
    { { { { -3981, 0, 962 }, { 213, 3993, 882 }, { -938, 908, -3882 } }, { -784, 2080, 8975 } }, 275 },
    { { { { -4071, 0, 448 }, { 31, 4085, 288 }, { -447, 290, -4061 } }, { -1306, 2162, 2495 } }, 269 },
    { { { { -2233, 0, -3433 }, { -683, 4013, 444 }, { 3364, 815, -2188 } }, { -62, 1858, 8634 } }, 269 },
    { { { { -2474, 0, 3264 }, { 649, 4014, 492 }, { -3199, 814, -2424 } }, { -362, 1983, 8329 } }, 269 },
    { { { { 3975, 0, 986 }, { 54, 4089, -219 }, { -984, 225, 3969 } }, { -665, 2036, 0x2EF4 } }, 269 },
    { { { { 4034, 0, 705 }, { 123, 4032, -707 }, { -694, 718, 3972 } }, { -564, 1895, 6271 } }, 257 },
    { { { { 1839, 0, -3659 }, { -944, 3957, -474 }, { 3535, 1056, 1777 } }, { 945, 1694, 2927 } }, 282 },
    { { { { 2080, 0, 3528 }, { 535, 4048, -316 }, { -3487, 622, 2056 } }, { -392, 1529, 3409 } }, 282 },
};

SpriteBatch D_shelter_b1_main_corridor_8018427C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b1_main_corridor_8018428C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b1_main_corridor_8018429C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_shelter_b1_main_corridor_801842AC[56] = {
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 48, 56, 1642, { .fields = { 104, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -16, 64, 1482, { .fields = { 104, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -104, 56, 1484, { .fields = { 120, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -104, 32, 1375, { .fields = { 120, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -120, 32, 1446, { .fields = { 112, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 136, 24, 1691, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 136, 48, 1665, { .fields = { 120, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 144, 48, 1625, { .fields = { 112, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -128, -24, 1509, { .fields = { 88, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -128, -16, 1510, { .fields = { 88, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -128, -8, 1520, { .fields = { 88, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -128, 0, 1426, { .fields = { 112, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -128, 16, 1432, { .fields = { 112, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -144, 24, 1443, { .fields = { 88, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -144, 16, 1440, { .fields = { 96, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -144, 8, 1437, { .fields = { 96, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -144, 0, 1434, { .fields = { 88, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -144, 32, 1446, { .fields = { 96, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -144, 40, 1449, { .fields = { 96, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -128, 32, 1438, { .fields = { 120, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -144, 48, 1431, { .fields = { 96, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -144, 56, 1401, { .fields = { 96, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -128, 48, 1444, { .fields = { 120, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -144, 72, 1373, { .fields = { 96, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -144, 64, 1372, { .fields = { 96, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -128, 64, 1450, { .fields = { 120, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -112, 56, 1456, { .fields = { 120, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -120, 56, 1451, { .fields = { 120, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -120, 72, 1458, { .fields = { 88, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 144, -24, 1690, { .fields = { 88, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 144, 0, 1681, { .fields = { 88, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 144, 8, 1666, { .fields = { 88, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -40, 64, 1578, { .fields = { 80, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -40, 72, 1596, { .fields = { 80, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -64, 64, 1562, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -64, 72, 1597, { .fields = { 80, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -96, 64, 1546, { .fields = { 72, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 88, 56, 1644, { .fields = { 96, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 88, 64, 1668, { .fields = { 96, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 64, 56, 1655, { .fields = { 96, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 64, 64, 1651, { .fields = { 96, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -120, 0, 1479, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -120, 8, 1467, { .fields = { 104, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -120, 16, 1452, { .fields = { 104, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -120, 24, 1442, { .fields = { 104, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 144, -16, 1685, { .fields = { 112, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 144, 16, 1638, { .fields = { 104, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 144, 32, 1631, { .fields = { 104, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -144, -24, 1427, { .fields = { 96, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -144, -16, 1428, { .fields = { 96, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -144, -8, 1431, { .fields = { 96, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -96, 72, 1574, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -80, 72, 1584, { .fields = { 96, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 128, 56, 1674, { .fields = { 112, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 120, 56, 1668, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 112, 56, 1646, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b1_main_corridor_8018470C[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 56, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

GpDrawAreaRec D_shelter_b1_main_corridor_80184724[2] = {
    { { 33, 0, 285, 239 }, 1373 },
    { { 0, 0, 0, 0 }, 0xFFFF },
};

GpSprtElem D_shelter_b1_main_corridor_80184738[13] = {
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -120, 96, 694, { .fields = { 120, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -128, 96, 712, { .fields = { 120, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -136, 96, 712, { .fields = { 120, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -144, 96, 685, { .fields = { 120, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -160, 96, 675, { .fields = { 112, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -136, 72, 854, { .fields = { 112, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -160, 72, 682, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -160, 48, 679, { .fields = { 104, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -160, 32, 650, { .fields = { 96, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -160, 8, 647, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -160, -16, 650, { .fields = { 112, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -160, -32, 651, { .fields = { 112, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -160, -48, 679, { .fields = { 120, 232 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b1_main_corridor_8018483C[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 13, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_shelter_b1_main_corridor_80184854[17] = {
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 136, 104, 0xFFF0, { .fields = { 120, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 104, 104, 732, { .fields = { 120, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 112, 104, 730, { .fields = { 120, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 112, 88, 759, { .fields = { 120, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 120, 104, 746, { .fields = { 120, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 120, 80, 802, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 128, 104, 779, { .fields = { 112, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 128, 88, 773, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 128, 64, 971, { .fields = { 120, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 136, 72, 754, { .fields = { 120, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 144, 72, 728, { .fields = { 112, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 136, 56, 769, { .fields = { 104, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 136, 40, 773, { .fields = { 104, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 136, 24, 750, { .fields = { 104, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 136, 8, 752, { .fields = { 104, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 144, -8, 747, { .fields = { 112, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 144, -32, 748, { .fields = { 112, 48 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b1_main_corridor_801849A8[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 17, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_shelter_b1_main_corridor_801849C0[24] = {
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 64, 48, 1953, { .fields = { 104, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 64, 40, 2152, { .fields = { 104, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 80, 40, 2102, { .fields = { 112, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 96, 40, 2073, { .fields = { 112, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 112, 40, 2045, { .fields = { 112, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 128, 40, 2024, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 136, 40, 1995, { .fields = { 112, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 144, 40, 1928, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 144, 24, 1922, { .fields = { 104, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 144, 8, 1955, { .fields = { 112, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 152, -16, 1961, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 8, 48, 0xFFF0, { .fields = { 104, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 8, 40, 2257, { .fields = { 104, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -8, 32, 2293, { .fields = { 120, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 0, 40, 2267, { .fields = { 120, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -24, 32, 2308, { .fields = { 112, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -32, 32, 2335, { .fields = { 120, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -40, 32, 2369, { .fields = { 120, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -48, 24, 2362, { .fields = { 104, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -64, 32, 2311, { .fields = { 104, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -64, 24, 2315, { .fields = { 104, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -48, 8, 2351, { .fields = { 120, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -64, 8, 2324, { .fields = { 112, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -64, -16, 2289, { .fields = { 104, 24 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b1_main_corridor_80184BA0[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 24, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

GpDrawAreaRec D_shelter_b1_main_corridor_80184BB8[2] = {
    { { 105, 0, 215, 239 }, 2289 },
    { { 0, 0, 0, 0 }, 0xFFFF },
};

GpSprtElem D_shelter_b1_main_corridor_80184BCC[19] = {
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 136, 88, 0, { .fields = { 88, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 56, 112, 0, { .fields = { 96, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 56, 96, 752, { .fields = { 104, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 56, 88, 802, { .fields = { 96, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 72, 88, 783, { .fields = { 112, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 88, 88, 774, { .fields = { 112, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 104, 88, 767, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 120, 88, 758, { .fields = { 112, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 136, 96, 750, { .fields = { 104, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -72, 88, 838, { .fields = { 88, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -64, 72, 881, { .fields = { 96, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -72, 72, 881, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -88, 72, 874, { .fields = { 112, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -104, 72, 886, { .fields = { 112, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -120, 72, 897, { .fields = { 112, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -136, 72, 909, { .fields = { 112, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -160, 64, 984, { .fields = { 88, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -144, 72, 919, { .fields = { 120, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -160, 72, 925, { .fields = { 96, 16 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b1_main_corridor_80184D48[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 19, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_shelter_b1_main_corridor_80184D60[26] = {
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -144, 88, 0, { .fields = { 80, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -112, 40, 731, { .fields = { 96, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -120, 24, 744, { .fields = { 88, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -128, 8, 734, { .fields = { 104, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -128, -8, 731, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -136, -24, 729, { .fields = { 88, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -144, -32, 725, { .fields = { 80, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -144, -48, 721, { .fields = { 80, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -144, -56, 717, { .fields = { 80, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -152, -72, 714, { .fields = { 88, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -160, -80, 726, { .fields = { 72, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -160, -96, 723, { .fields = { 88, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -160, -104, 717, { .fields = { 80, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -160, -120, 712, { .fields = { 112, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -160, -72, 805, { .fields = { 120, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -160, -56, 780, { .fields = { 112, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -160, -40, 843, { .fields = { 112, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -160, -24, 850, { .fields = { 104, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -160, -8, 858, { .fields = { 96, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -160, 8, 745, { .fields = { 96, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, -160, 24, 881, { .fields = { 88, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 16 } }, -160, 40, 889, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -160, 56, 923, { .fields = { 112, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -144, 56, 906, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -120, 56, 743, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 24 } }, -120, 72, 843, { .fields = { 88, 56 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b1_main_corridor_80184F68[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 26, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_shelter_b1_main_corridor_80184F80[20] = {
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -128, 112, 610, { .fields = { 88, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -128, 104, 627, { .fields = { 88, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -128, 96, 663, { .fields = { 96, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -136, 88, 777, { .fields = { 120, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -128, 88, 672, { .fields = { 96, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -128, 80, 718, { .fields = { 104, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -128, 72, 771, { .fields = { 112, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -136, 72, 780, { .fields = { 120, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -128, 56, 937, { .fields = { 120, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -136, 56, 736, { .fields = { 120, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -144, 56, 677, { .fields = { 120, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -160, 48, 735, { .fields = { 104, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -144, 40, 735, { .fields = { 104, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -144, 16, 743, { .fields = { 112, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -160, 24, 739, { .fields = { 112, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -160, 0, 697, { .fields = { 112, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -144, -16, 708, { .fields = { 120, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -160, -16, 701, { .fields = { 104, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -160, -40, 685, { .fields = { 104, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -160, -72, 704, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b1_main_corridor_80185110[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 20, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

GpSprtRec D_shelter_b1_main_corridor_80185128[10] = {
    { { .empty = D_shelter_b1_main_corridor_8018427C }, D_shelter_b1_main_corridor_8018427C, NULL },
    { { .empty = D_shelter_b1_main_corridor_8018428C }, D_shelter_b1_main_corridor_8018428C, NULL },
    { { .empty = D_shelter_b1_main_corridor_8018429C }, D_shelter_b1_main_corridor_8018429C, NULL },
    { { .elements = D_shelter_b1_main_corridor_801842AC }, D_shelter_b1_main_corridor_8018470C, D_shelter_b1_main_corridor_80184724 },
    { { .elements = D_shelter_b1_main_corridor_80184738 }, D_shelter_b1_main_corridor_8018483C, NULL },
    { { .elements = D_shelter_b1_main_corridor_80184854 }, D_shelter_b1_main_corridor_801849A8, NULL },
    { { .elements = D_shelter_b1_main_corridor_801849C0 }, D_shelter_b1_main_corridor_80184BA0, D_shelter_b1_main_corridor_80184BB8 },
    { { .elements = D_shelter_b1_main_corridor_80184BCC }, D_shelter_b1_main_corridor_80184D48, NULL },
    { { .elements = D_shelter_b1_main_corridor_80184D60 }, D_shelter_b1_main_corridor_80184F68, NULL },
    { { .elements = D_shelter_b1_main_corridor_80184F80 }, D_shelter_b1_main_corridor_80185110, NULL },
};

GpPointLight D_shelter_b1_main_corridor_801851A0[6] = {
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 2129, -1643, -681 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2638, 2638, 2688, { 0, 0 } }, 2000, 6000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 88, -1643, -6981 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2638, 2638, 2688, { 0, 0 } }, 2000, 6000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 2129, -1643, -0x2CF5 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2638, 2638, 2688, { 0, 0 } }, 2000, 6000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -73, -1489, -0x40FD } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2197, 2212, 2233, { 0, 0 } }, 2944, 8822 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -2366, -1643, -0x2CF5 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2638, 2638, 2688, { 0, 0 } }, 2000, 6000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -2366, -1643, -681 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2638, 2638, 2688, { 0, 0 } }, 2000, 6000 },
};

GpRoomCoordSet D_shelter_b1_main_corridor_801853E0 = { 0, NULL, 6, D_shelter_b1_main_corridor_801851A0, 0, NULL };

GpObj4C D_shelter_b1_main_corridor_801853F8[16] = {
    { NULL, NULL, NULL, { 64, -1568, -0x2EE0, 0 }, { { -2316, -3712, 337, 0 }, { 2305, -3712, -348, 0 }, { -2316, 3712, 337, 0 }, { 2305, 3712, -348, 0 } }, { -603, 0, -4063, 0 }, { 0, 0, 4096, 0 }, 4374, 0, 3, 4, 1, 0 },
    { NULL, NULL, NULL, { -32, -1696, -0x2F5D, 0 }, { { 2562, -3712, -382, 0 }, { -2565, -3712, 379, 0 }, { 2562, 3712, -382, 0 }, { -2565, 3712, 379, 0 } }, { 601, 0, 4055, 0 }, { 0, 0, 4096, 0 }, 4521, 0, 4, 3, 1, 0 },
    { NULL, NULL, NULL, { 128, -1568, -7200, 0 }, { { 2592, -3712, 128, 0 }, { -2592, -3712, -128, 0 }, { 2592, 3712, 128, 0 }, { -2592, 3712, -128, 0 } }, { -203, 0, 4100, 0 }, { 0, 0, 4096, 0 }, 4521, 0, 7, 4, 1, 0 },
    { NULL, NULL, NULL, { 112, -1664, -7056, 0 }, { { -2640, -3712, -80, 0 }, { 2640, -3712, 80, 0 }, { -2640, 3712, -80, 0 }, { 2640, 3712, 80, 0 } }, { 124, 0, -4104, 0 }, { 0, 0, 4096, 0 }, 4550, 0, 4, 7, 1, 0 },
    { NULL, NULL, NULL, { 0, -1664, -3456, 0 }, { { -2640, -3712, -80, 0 }, { 2640, -3712, 80, 0 }, { -2640, 3712, -80, 0 }, { 2640, 3712, 80, 0 } }, { 124, 0, -4104, 0 }, { 0, 0, 4096, 0 }, 4550, 0, 7, 8, 1, 0 },
    { NULL, NULL, NULL, { 0, -1536, -3616, 0 }, { { 2592, -3712, 128, 0 }, { -2592, -3712, -128, 0 }, { 2592, 3712, 128, 0 }, { -2592, 3712, -128, 0 } }, { -203, 0, 4100, 0 }, { 0, 0, 4096, 0 }, 4521, 0, 8, 7, 1, 0 },
    { NULL, NULL, NULL, { 2432, -1632, -9952, 0 }, { { -432, -3712, -2106, 0 }, { 424, -3712, 2099, 0 }, { -432, 3712, -2106, 0 }, { 424, 3712, 2099, 0 } }, { 4018, 0, -819, 0 }, { 0, 0, 4096, 0 }, 4283, 0, 5, 4, 1, 0 },
    { NULL, NULL, NULL, { 2576, -1632, -9936, 0 }, { { 317, -3712, 1745, 0 }, { -329, -3712, -1755, 0 }, { 317, 3712, 1745, 0 }, { -329, 3712, -1755, 0 } }, { -4037, 0, 744, 0 }, { 0, 0, 4096, 0 }, 4096, 0, 4, 5, 1, 0 },
    { NULL, NULL, NULL, { -2208, -1568, -0x2740, 0 }, { { -544, -3712, 1691, 0 }, { 536, -3712, -1700, 0 }, { -544, 3712, 1691, 0 }, { 536, 3712, -1700, 0 } }, { -3911, 0, -1246, 0 }, { 0, 0, 4096, 0 }, 4096, 0, 6, 4, 1, 0 },
    { NULL, NULL, NULL, { -2337, -1600, -0x27A1, 0 }, { { 466, -3712, -1686, 0 }, { -475, -3712, 1674, 0 }, { 466, 3712, -1686, 0 }, { -475, 3712, 1674, 0 } }, { 3950, 0, 1106, 0 }, { 0, 0, 4096, 0 }, 4096, 0, 4, 6, 1, 0 },
    { NULL, NULL, NULL, { 1568, -1600, -1776, 0 }, { { 48, -3712, -1472, 0 }, { -48, -3712, 1472, 0 }, { 48, 3712, -1472, 0 }, { -48, 3712, 1472, 0 } }, { 4107, 0, 133, 0 }, { 0, 0, 4096, 0 }, 3990, 0, 9, 8, 1, 0 },
    { NULL, NULL, NULL, { 1664, -1568, -1824, 0 }, { { -58, -3712, 1401, 0 }, { 44, -3712, -1413, 0 }, { -58, 3712, 1401, 0 }, { 44, 3712, -1413, 0 } }, { -4101, 0, -149, 0 }, { 0, 0, 4096, 0 }, 3965, 0, 8, 9, 1, 0 },
    { NULL, NULL, NULL, { -2144, -1536, -1952, 0 }, { { 206, -3712, 1372, 0 }, { -244, -3712, -1407, 0 }, { 206, 3712, 1372, 0 }, { -244, 3712, -1407, 0 } }, { -4050, 0, 655, 0 }, { 0, 0, 4096, 0 }, 3965, 0, 10, 8, 1, 0 },
    { NULL, NULL, NULL, { -2272, -1536, -1920, 0 }, { { -195, -3712, -1487, 0 }, { 156, -3712, 1434, 0 }, { -195, 3712, -1487, 0 }, { 156, 3712, 1434, 0 } }, { 4075, 0, -491, 0 }, { 0, 0, 4096, 0 }, 3990, 0, 8, 10, 1, 0 },
    { NULL, NULL, NULL, { 0, -1696, -0x3C61, 0 }, { { -2293, -3712, 454, 0 }, { 2289, -3712, -457, 0 }, { -2293, 3712, 454, 0 }, { 2289, 3712, -457, 0 } }, { -802, 0, -4028, 0 }, { 0, 0, 4096, 0 }, 4374, 0, 2, 3, 1, 0 },
    { NULL, NULL, NULL, { 0, -1696, -0x3D21, 0 }, { { 2538, -3712, -511, 0 }, { -2546, -3712, 502, 0 }, { 2538, 3712, -511, 0 }, { -2546, 3712, 502, 0 } }, { 801, 0, 4021, 0 }, { 0, 0, 4096, 0 }, 4521, 0, 3, 2, 129, 0 },
};

GpObj4C D_shelter_b1_main_corridor_801858B8[6] = {
    { NULL, NULL, NULL, { 0, -48, -0x4990, 0 }, { { -1024, 0, -560, 0 }, { 1024, 0, -560, 0 }, { -1024, 0, 560, 0 }, { 1024, 0, 560, 0 } }, { 0, 4110, 0, 0 }, { 0, 0, 4096, 0 }, 1166, 0, 9, 18, 2, 0 },
    { NULL, NULL, NULL, { -3488, -48, -9984, 0 }, { { 560, 0, -1024, 0 }, { 560, 0, 1024, 0 }, { -560, 0, -1024, 0 }, { -560, 0, 1024, 0 } }, { 0, 4110, 0, 0 }, { 4096, 0, 0, 0 }, 1166, 0, 25, 33, 2, 0 },
    { NULL, NULL, NULL, { 3456, -48, -9952, 0 }, { { 560, 0, -1024, 0 }, { 560, 0, 1024, 0 }, { -560, 0, -1024, 0 }, { -560, 0, 1024, 0 } }, { 0, 4110, 0, 0 }, { -4096, 0, 0, 0 }, 1166, 0, 13, 49, 2, 0 },
    { NULL, NULL, NULL, { -3456, -48, -2016, 0 }, { { 560, 0, -1024, 0 }, { 560, 0, 1024, 0 }, { -560, 0, -1024, 0 }, { -560, 0, 1024, 0 } }, { 0, 4110, 0, 0 }, { 4096, 0, 0, 0 }, 1166, 0, 24, 65, 2, 0 },
    { NULL, NULL, NULL, { 3456, -48, -2048, 0 }, { { 560, 0, -1024, 0 }, { 560, 0, 1024, 0 }, { -560, 0, -1024, 0 }, { -560, 0, 1024, 0 } }, { 0, 4110, 0, 0 }, { -4096, 0, 0, 0 }, 1166, 0, 14, 81, 2, 0 },
    { NULL, NULL, NULL, { 0, -48, 0, 0 }, { { -1024, 0, -560, 0 }, { 1024, 0, -560, 0 }, { -1024, 0, 560, 0 }, { 1024, 0, 560, 0 } }, { 0, 4110, 0, 0 }, { 0, 0, -4096, 0 }, 1166, 0, 16, 97, 130, 0 },
};

GpAreaTmdRec D_shelter_b1_main_corridor_80185A80[3] = {
    { 21, 21, 0, 0, { 0, 0 }, D_80135C30 },
    { 11, 11, 1, 0, { 0, 0 }, D_8015F400 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_shelter_b1_main_corridor_80185AA4[2] = {
    { 6, 6, 3, 0, { 0, 0 }, D_80151B10 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_shelter_b1_main_corridor_80185ABC[2] = {
    { 3, 3, 0, 0, { 0, 0 }, D_80148110 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_shelter_b1_main_corridor_80185AD4[2] = {
    { 22, 22, 3, 0, { 0, 0 }, D_80154188 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_shelter_b1_main_corridor_80185AEC[3] = {
    { 56, 56, 0, 0, { 0, 0 }, D_801482C0 },
    { 57, 57, 1, 0, { 0, 0 }, D_801611F8 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

AreaPlacement D_shelter_b1_main_corridor_80185B10[6] = {
    { 21, 34, 2048, -3600, -1750, -7800, 1024, 0, 0, 2, 2 },
    { 21, 4, 0, 1100, -2100, -0x3C8C, 0, 0, 0, 2, 3 },
    { 21, 4, 0, -1100, -2100, -0x3C8C, 0, 0, 0, 2, 3 },
    { 11, 1, 0, 0, 0, -2000, 2048, 0, 2, 4, 0 },
    { 11, 1, 0, 0, 0, -0x2710, 0, 0, 2, 4, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaPlacement D_shelter_b1_main_corridor_80185B70[4] = {
    { 6, 0, 1, 0, 0, -4000, 0, 0, 0, 2, 0 },
    { 6, 0, 1, 0, 0, -8500, 2048, 0, 0, 2, 0 },
    { 6, 0, 1, 0, 0, -0x38A4, 2048, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaPlacement D_shelter_b1_main_corridor_80185BB0[3] = {
    { 3, 0, 0, 0, 0, -3000, 2048, 0, 0, 2, 0 },
    { 3, 0, 1, 0, 0, -0x2710, 0, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaPlacement D_shelter_b1_main_corridor_80185BE0[2] = {
    { 22, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaPlacement D_shelter_b1_main_corridor_80185C00[3] = {
    { 56, 6, 1, 0, 0, -2000, 2048, 0, 0, 2, 0 },
    { 57, 4, 1, -2000, 0, -0x2710, 1024, 0, 3, 5, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

GpAreaVariant D_shelter_b1_main_corridor_80185C30[22] = {
    { NULL, NULL },
    { D_shelter_b1_main_corridor_80185B10, D_shelter_b1_main_corridor_80185A80 },
    { D_shelter_b1_main_corridor_80185B70, D_shelter_b1_main_corridor_80185AA4 },
    { D_shelter_b1_main_corridor_80185BB0, D_shelter_b1_main_corridor_80185ABC },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { D_shelter_b1_main_corridor_80185BE0, D_shelter_b1_main_corridor_80185AD4 },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { D_shelter_b1_main_corridor_80185C00, D_shelter_b1_main_corridor_80185AEC },
    { NULL, NULL },
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

s32 D_shelter_b1_main_corridor_80185CE0[3] = {
    0x10000059,
    0x1000005B,
    0x10000059,
};

GpRoomParamRec D_shelter_b1_main_corridor_80185CEC[1] = {
    { 0, 0, 1, 0, NULL },
};

GpRoomParamRec D_shelter_b1_main_corridor_80185CF4[1] = {
    { 0, 1, 0, 0, NULL },
};

GpRoomParamRec D_shelter_b1_main_corridor_80185CFC[1] = {
    { 0, 0, 1, 0, D_shelter_b1_main_corridor_80185CE0 },
};

GpRoomParamRec* D_shelter_b1_main_corridor_80185D04[8] = {
    D_shelter_b1_main_corridor_80185CEC,
    D_shelter_b1_main_corridor_80185CF4,
    D_shelter_b1_main_corridor_80185CFC,
    D_shelter_b1_main_corridor_80185CEC,
    D_shelter_b1_main_corridor_80185CEC,
    D_shelter_b1_main_corridor_80185CEC,
    D_shelter_b1_main_corridor_80185CEC,
    D_shelter_b1_main_corridor_80185CEC,
};

RoomFadeStorage D_shelter_b1_main_corridor_80185D24 = { 0 };

RoomEventMsg D_shelter_b1_main_corridor_80185D2C = { 0 };

ShelterB1MainCorridorStorage5D34 D_shelter_b1_main_corridor_80185D34 = { 0 };

RoomEventMsg D_shelter_b1_main_corridor_80185D3C = { 0 };

u8 D_shelter_b1_main_corridor_80185D44[4] = {
    0,
    7,
    132,
    0,
};

ShelterB1MainCorridorStorage5D48 D_shelter_b1_main_corridor_80185D48;

RoomLatchedEvent D_shelter_b1_main_corridor_80185D68;

static s32            func_shelter_b1_main_corridor_8017D620(RoomEventReq* req, RoomEventMsg* msg);
static __inline__ s32 _corridorStartEvent(RoomEventMsg* dst, RoomLatchedEvent* event);
static void           func_shelter_b1_main_corridor_8017DD4C(Task* task);
static void           func_shelter_b1_main_corridor_8017DD90(Task* task);
static void           func_shelter_b1_main_corridor_8017F064(GfxCoord* arg0, s32 arg1, s32 arg2, u8* rgb);
static void           func_shelter_b1_main_corridor_8017F488(GfxCoord* arg0, s16 arg1, u8* arg2);

/// The corridor's event gate: given a request and the incoming message,
/// answers whether the event fires. A nibble already in its fired state (set,
/// or clear for a negative `flagId`) answers 1. A missing collected-bit
/// prerequisite answers 0 and runs the request's CAP command. Otherwise the
/// message and request are latched, the nibble is written and the event task
/// is spawned, for 2. A non-zero `field_5` on the message only asks for the
/// answer and suppresses every side effect.
static s32 func_shelter_b1_main_corridor_8017D620(RoomEventReq* req, RoomEventMsg* msg)
{
    s32 flag;
    s32 id;
    s32 mode;
    s32 got;
    s32 ret;
    s32 neg;

    flag                                      = req->flagId;
    D_shelter_b1_main_corridor_80185D34.value = 0;
    neg                                       = flag < 0;
    got                                       = (s16)flag;
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
                D_shelter_b1_main_corridor_80185D2C       = *msg;
                D_shelter_b1_main_corridor_80185D48.value = *req;
                id                                        = req->flagId;
                mode                                      = 1;
                if (id < 0) {
                    id   = -id;
                    mode = 0;
                }
                GameFlag_SetNibble(id, mode);
                Task_SpawnFromTable(&D_shelter_b1_main_corridor_8018308C, 0, 0, 0);
                D_shelter_b1_main_corridor_80185D34.value = 1;
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

/// Event task the gate spawns: runs the latched request's CAP command, plays
/// its two sounds in turn (each optional) and waits for each to finish, then
/// queues sound event 0x80000000, records the latched message's area, warp and
/// room in the save data's location, spawns task 0x11 and kills itself.
void func_shelter_b1_main_corridor_8017D784(Task* task)
{
    switch (task->state) {
        case 0:
            Gp_StateF0.field_4 = 1;
            Gp_MsgPlayerWeapon(0);
            Gp_RunCapCmd1(D_shelter_b1_main_corridor_80185D48.value.field_0);
            if (D_shelter_b1_main_corridor_80185D48.value.field_8 != 0) {
                SndEvt_EnqueueType6(D_shelter_b1_main_corridor_80185D48.value.field_8, 0, 0);
                task->state++;
            } else {
                task->state = 2;
            }
            break;
        case 1:
            if (SndVoice_HasActiveId(D_shelter_b1_main_corridor_80185D48.value.field_8) == 0) {
                task->state++;
            }
            break;
        case 2:
            task->state++;
            break;
        case 3:
            if (D_shelter_b1_main_corridor_80185D48.value.field_C != 0) {
                SndEvt_EnqueueType6(D_shelter_b1_main_corridor_80185D48.value.field_C, 0, 0);
                task->state++;
            } else {
                task->state = 5;
            }
            break;
        case 4:
            if (SndVoice_HasActiveId(D_shelter_b1_main_corridor_80185D48.value.field_C) == 0) {
                task->state++;
            }
            break;
        case 5:
            SndEvt_EnqueueType7(0x80000000, 0);
            gDisplayState.spriteVariant       = 1;
            Mc_SaveData[0].state.at4.loc.area = D_shelter_b1_main_corridor_80185D2C.prefix.packed;
            Mc_SaveData[0].state.at4.loc.warp = D_shelter_b1_main_corridor_80185D2C.field_2;
            Mc_SaveData[0].state.at4.loc.room = (u8)D_shelter_b1_main_corridor_80185D2C.field_3;
            Task_Spawn(0, 0x11, 0, 0);
            taskKill(task);
            break;
    }
}

/// The event task the message handler spawns: runs the latched event's CAP
/// command and waits for it, spawns task 0x31 if the event asks for it, plays
/// the event's stage sound (if any) and waits for it to end, then queues sound
/// event 0x80000000, records the latched message's area, warp and room in the
/// save data's location, spawns task 0x11 and kills itself.
void func_shelter_b1_main_corridor_8017D8F4(Task* arg0)
{
    switch (arg0->state) {
        case 0:
            Gp_StateF0.field_4 = 1;
            Gp_MsgPlayerWeapon(0);
            Gp_RunCapCmd(D_shelter_b1_main_corridor_80185D68.capCmd, 0);
            D_80115690 = 1;
            arg0->state++;
            break;
        case 1:
            if (Gp_CapBusy() == 0) {
                if (D_shelter_b1_main_corridor_80185D68.fade != 0) {
                    D_shelter_b1_main_corridor_80185D24.fade.field_0 = 0;
                    D_shelter_b1_main_corridor_80185D24.fade.field_1 = 0;
                    D_shelter_b1_main_corridor_80185D24.fade.field_2 = 0x1E;
                    Task_Spawn(1, 0x31, 0, &D_shelter_b1_main_corridor_80185D24.fade);
                }
                arg0->state++;
            }
            break;
        case 2:
            if (D_shelter_b1_main_corridor_80185D68.stageSnd != 0) {
                Gp_EnqueueStageSnd6(D_shelter_b1_main_corridor_80185D68.stageSnd, 0, 0);
                arg0->state++;
            } else {
                arg0->state = 4;
            }
            break;
        case 3:
            if (SndVoice_HasActiveId(Gp_PackStageSndId(D_shelter_b1_main_corridor_80185D68.stageSnd)) == 0) {
                arg0->state++;
            }
            break;
        case 4:
            SndEvt_EnqueueType7(0x80000000, 0);
            gDisplayState.spriteVariant       = 1;
            Mc_SaveData[0].state.at4.loc.area = D_shelter_b1_main_corridor_80185D3C.prefix.packed;
            Mc_SaveData[0].state.at4.loc.warp = D_shelter_b1_main_corridor_80185D3C.field_2;
            Mc_SaveData[0].state.at4.loc.room = (u8)D_shelter_b1_main_corridor_80185D3C.field_3;
            Task_Spawn(0, 0x11, 0, 0);
            taskKill(arg0);
            break;
    }
}

/// Starts `event` for the outgoing message `dst` unless its flag says it has
/// already happened (answering 1). Otherwise answers 2, and - unless
/// `dst->field_5` asks for a dry run - latches the message and the event,
/// sets the flag and spawns the room's event task.
static __inline__ s32 _corridorStartEvent(RoomEventMsg* dst, RoomLatchedEvent* event)
{
    D_shelter_b1_main_corridor_80185D44[0] = 0;
    if (GameFlag_GetNibble(event->flagId) == 0 || event->flagId == 0) {
        if (dst->field_5 == 0) {
            D_shelter_b1_main_corridor_80185D3C = *dst;
            D_shelter_b1_main_corridor_80185D68 = *event;
            if (event->flagId != 0) {
                GameFlag_SetNibble(event->flagId, 1);
            }
            Task_SpawnFromTable(&D_shelter_b1_main_corridor_80183098, 0, 0, 0);
            D_shelter_b1_main_corridor_80185D44[0] = 1;
        }
        return 2;
    }
    return 1;
}

/// Message handler: copies the incoming message to `out` and forwards both to
/// `func_map_shelter_80179A04`. Messages 0xD, 0xE, 0x10 and 0x19 start the room's events on
/// flags 0xEE, 0xEF, 0x12C and 0x12D; 0x18 does the same on flag 0x12E once
/// nibble 0xAC is set, and before that runs CAP command 1. Message 9 runs CAP
/// command 5 once nibble 0x7A reaches 6, and otherwise goes through the rooms'
/// event gate on flag 0xA5. Any other message answers 1.
s32 func_shelter_b1_main_corridor_8017DA8C(Task* task, s32 msgId, RoomEventMsg* in, RoomEventMsg* out)
{
    RoomEventReq     req;
    RoomLatchedEvent event;

    *out = *in;
    func_map_shelter_80179A04(in, out);
    if (in->prefix.packed == 0xD) {
        event.capCmd   = 3;
        event.stageSnd = 0x540F0001;
        event.flagId   = 0xEE;
        event.fade     = 0;
        return _corridorStartEvent(out, &event);
    }
    if (in->prefix.packed == 0xE) {
        event.capCmd   = 4;
        event.stageSnd = 0x540F0001;
        event.flagId   = 0xEF;
        event.fade     = 0;
        return _corridorStartEvent(out, &event);
    }
    if (in->prefix.packed == 0x10) {
        event.capCmd   = 6;
        event.stageSnd = 0x540F0001;
        event.flagId   = 0x12C;
        event.fade     = 0;
        return _corridorStartEvent(out, &event);
    }
    if (in->prefix.packed == 9) {
        if (GameFlag_GetNibble(0x7A) >= 6) {
            if (in->field_5 == 0) {
                Gp_RunCapCmd1(5);
            }
            return 0;
        }
        req.field_0 = 2;
        req.field_4 = 1;
        req.field_8 = 0;
        req.field_C = 0x540F0001;
        req.flagId  = 0xA5;
        req.itemId  = 0;
        return func_shelter_b1_main_corridor_8017D620(&req, out);
    }
    if (in->prefix.packed == 0x18) {
        if (GameFlag_GetNibble(0xAC) == 0) {
            if (in->field_5 == 0) {
                Gp_SetNibbleIf(in->field_6, 2);
                Gp_RunCapCmd1(1);
            }
            return 2;
        }
        event.capCmd   = 8;
        event.stageSnd = 0x540F0001;
        event.flagId   = 0x12E;
        event.fade     = 0;
        return _corridorStartEvent(out, &event);
    }
    if (in->prefix.packed == 0x19) {
        event.capCmd   = 7;
        event.stageSnd = 0x540F0001;
        event.flagId   = 0x12D;
        event.fade     = 0;
        return _corridorStartEvent(out, &event);
    }
    return 1;
}

s32 func_shelter_b1_main_corridor_8017DCEC(Task* task, s32 msgId, GpMessageArg arg2, GpMessageArg arg3)
{
    return 0;
}

s32 func_shelter_b1_main_corridor_8017DCF4(Task* task, s32 msgId, GpMessageArg arg2, GpMessageArg arg3)
{
    return 0;
}

s32 func_shelter_b1_main_corridor_8017DCFC(Task* task, s32 msgId, GpMessageArg arg2, GpMessageArg arg3)
{
    return 0;
}

s32 func_shelter_b1_main_corridor_8017DD04(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    s32 sndId;

    if (arg2 != 0xD) {
        if (arg2 == 0xE) {
            sndId = 0x540F0000 | 0xE;
            goto play;
        }
    } else {
        sndId = 0x540F000D;
    play:
        SndEvt_EnqueueType6(sndId, 0, 0);
    }
    return 0;
}

/// Installs the room's message table on `task`, registers the task in pointer
/// slot 7 and steps it to its next state.
static void func_shelter_b1_main_corridor_8017DD4C(Task* task)
{
    task->msgTable = D_shelter_b1_main_corridor_801830A4;
    Game_SetPtrSlot(task, 7);
    task->state = (s32)(task->state + 1);
}

/// Idle state of the room's message task.
static void func_shelter_b1_main_corridor_8017DD90(Task* task)
{
}

/// States of the room's message task: install the message table, idle, die.
static const TaskFuncTable3 D_shelter_b1_main_corridor_8017D5F0 = {
    { func_shelter_b1_main_corridor_8017DD4C, func_shelter_b1_main_corridor_8017DD90, taskKill },
};

/// Runs the room's message task: calls the state handler `task->state` selects
/// from a stack copy of its three-entry table.
void func_shelter_b1_main_corridor_8017DD98(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_shelter_b1_main_corridor_8017D5F0;
    sp.funcs[task->state](task);
}

/// Per-frame drawing task: on its first tick it sets seven gameplay effect ids,
/// then every tick draws the capsules and sprites of whichever camera view is
/// active (views 2-10).
void func_shelter_b1_main_corridor_8017DDF0(Task* arg0)
{
    if (arg0->state == 0) {
        D_80115728  = 0x60248;
        D_80115744  = 0x60254;
        D_8011573C  = 0x6025F;
        D_80115720  = 0x6026B;
        D_80115758  = 0x601CC;
        D_8011572C  = 0x601E8;
        D_80115750  = 0x60204;
        arg0->state = 1;
    }

    switch (Gp_GetViewIndex() & 0xFF) {
        case 2:
        case 3: {
            SVECTOR* p;
            p = D_shelter_b1_main_corridor_801830D4;
            func_shelter_b1_main_corridor_8017E070(&p[0], 0x200, 0x800, 0x111);
            func_shelter_b1_main_corridor_8017E070(&p[2], 0x200, 0x800, 0x111);
            break;
        }
        case 4: {
            SVECTOR* p;
            p = D_shelter_b1_main_corridor_80183114;
            func_shelter_b1_main_corridor_8017E070(&p[0], 0x200, 0x800, 0x10);
            func_shelter_b1_main_corridor_8017E070(&p[4], 0x200, 0, 0x10);
            func_shelter_b1_main_corridor_8017E858(&p[10], 1, 0x300);
            func_shelter_b1_main_corridor_8017E858(&p[11], 1, 0x300);
            func_shelter_b1_main_corridor_8017E858(&p[17], 1, 0x300);
            func_shelter_b1_main_corridor_8017E858(&p[18], 1, 0x300);
            break;
        }
        case 5:
            func_shelter_b1_main_corridor_8017E070(D_shelter_b1_main_corridor_80183134, 0x200, 0, 0x10);
            break;
        case 6:
            func_shelter_b1_main_corridor_8017E070(D_shelter_b1_main_corridor_80183114, 0x200, 0x800, 0x10);
            break;
        case 7: {
            SVECTOR* p;
            p = D_shelter_b1_main_corridor_80183124;
            func_shelter_b1_main_corridor_8017E070(&p[0], 0x200, 0x800, 0x10);
            func_shelter_b1_main_corridor_8017E070(&p[4], 0x200, 0, 0x10);
            func_shelter_b1_main_corridor_8017E070(&p[6], 0x200, 0x800, 0x100);
            func_shelter_b1_main_corridor_8017E858(&p[13], 1, 0x300);
            func_shelter_b1_main_corridor_8017E858(&p[14], 1, 0x300);
            func_shelter_b1_main_corridor_8017E858(&p[20], 1, 0x300);
            func_shelter_b1_main_corridor_8017E858(&p[21], 1, 0x300);
            break;
        }
        case 8: {
            SVECTOR* p;
            p = D_shelter_b1_main_corridor_80183124;
            func_shelter_b1_main_corridor_8017E070(&p[0], 0x200, 0x800, 0x10);
            func_shelter_b1_main_corridor_8017E070(&p[6], 0x200, 0x800, 0x100);
            break;
        }
        case 9:
            func_shelter_b1_main_corridor_8017E070(D_shelter_b1_main_corridor_80183144, 0x200, 0, 0x10);
            break;
        case 10:
            func_shelter_b1_main_corridor_8017E070(D_shelter_b1_main_corridor_80183124, 0x200, 0x800, 0x10);
            break;
    }
}

/// Draws a gouraud capsule between the view-space points `arg0[0]` and
/// `arg0[1]`: a half-disc at each end, radius `(s16)arg1 * 64` over the end's
/// OTZ, joined by a band, all rotated by `(s16)arg2`. The inner vertices take
/// the colour coded in `arg3` (red from bits 8-15, green from bit 4, blue from
/// bit 0), each scaled by a blend byte that pulses with the frame counter, and
/// the outer rim is black. Nothing is drawn when the second point's OTZ is
/// below 0x11.
static void func_shelter_b1_main_corridor_8017E070(SVECTOR* arg0, s32 arg1, s32 arg2, s32 arg3)
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
            prim           = gGpuPrimCursor;
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
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz0 << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz0);

            prim           = gGpuPrimCursor;
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
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz0 << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz0);

            t3             = ang - 0x1000;
            prim           = gGpuPrimCursor;
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
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz1 << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz1);
        } while (ang < 0x800);
    }
    SCRATCH_POP_BYTES(0x18);
}

/// Draws one semi-transparent textured sprite centred on the view-space point
/// `arg0` when its OTZ is at least 0x11. `arg1` picks the 40-texel-wide cell
/// and its palette; the half-size is `(s16)arg2 * 39` over the OTZ. The grey
/// level pulses between 0x20 and 0x30 with the frame counter.
static void func_shelter_b1_main_corridor_8017E858(SVECTOR* arg0, s32 arg1, s32 arg2)
{
    RoomDraw25Scratch* block;
    POLY_FT4*          prim;
    s32                blend;
    s32                idx;
    u8                 frame;

    block = SCRATCH_PUSH(RoomDraw25Scratch);
    gte_SetTransMatrix(&gGfxViewCoord.workm);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_ldv0(arg0);
    gte_rtps();
    prim           = gGpuPrimCursor;
    gGpuPrimCursor = prim + 1;
    setPolyFT4(prim);
    gte_stsxy(&block->sx);
    gte_stszotz(&block->otz);
    if (block->otz >= 0x11) {
        idx         = (s16)arg1;
        frame       = gDisplayState.animFrame;
        prim->tpage = 0x2B;
        prim->clut  = (idx & 0x3F) | 0x4380;
        setUVWH(prim, idx * 40, 0, 0x27, 0x27);
        blend = ((frame & 1) << 4) + 0x20;
        setRGB0(prim, blend, blend, blend);
        setSemiTrans(prim, 1);
        block->radius = ((s16)arg2 * 39) / block->otz;
        prim->x0 = prim->x2 = block->sx - block->radius;
        prim->x1 = prim->x3 = block->sx + block->radius;
        prim->y0 = prim->y1 = block->sy - block->radius;
        prim->y2 = prim->y3 = block->sy + block->radius;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)), prim);
    }
    SCRATCH_POP(RoomDraw25Scratch);
}

/// A drifting mote. On its first tick it unpacks `spawnArg1` (see
/// `RoomMoteArg`): with either of the low two bits set it starts at full
/// brightness 0x80, moves along y at `speed` (negated when bit 1 is set) and
/// holds that brightness; with neither set it starts at 0x20, moves along y at
/// -(`speed` + a random 0-0x3F), and brightens by 0x20 a tick up to
/// 0x80. Every tick it moves its coordinate, every other tick it draws with an
/// advancing phase, and once its age passes `lifetime - 8` it dims by 0x10 a
/// tick. It releases its work block when dark, or when the room's event state
/// reaches 4.
void func_shelter_b1_main_corridor_8017EAD4(Task* task)
{
    GpEffWork* work;
    GfxCoord*  coord;
    s32        lifetime;

    work  = task->spawnArg2.pointer;
    coord = task->extra.coordBody->coord;
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
                coord->coord.t[1]  += work->move.vy;
                coord->composeStamp = GRAPHICS_COORD_DIRTY;
                Gp_UpdateCoord(coord);
                if (work->age & 1) {
                    work->index++;
                    func_shelter_b1_main_corridor_8017EDA0(coord, work->index, work->angle | 0x1000, work->scale | work->period);
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
                coord->coord.t[1]  += work->move.vy;
                coord->composeStamp = GRAPHICS_COORD_DIRTY;
                Gp_UpdateCoord(coord);
                if (work->age & 1) {
                    work->index++;
                    func_shelter_b1_main_corridor_8017EDA0(coord, work->index, work->angle, work->scale | work->period);
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
static void func_shelter_b1_main_corridor_8017EDA0(GfxCoord* arg0, u16 arg1, u16 arg2, u16 arg3)
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
        prim           = gGpuPrimCursor;
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
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << ds->otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                prim);
    }
    SCRATCH_POP(GpRingScratch);
}

/// Draws a flat ring of sixteen gouraud quads around the screen position of
/// `arg0`'s world translation, unless the projection flags an error. The
/// vertices at radius `(s16)arg1 * 64` over the OTZ are black and those at
/// `(s16)(arg1 + arg2) * 64` over the OTZ take `rgb`, so the ring shades from
/// black at `arg1` to the colour at `arg1 + arg2`.
static void func_shelter_b1_main_corridor_8017F064(GfxCoord* arg0, s32 arg1, s32 arg2, u8* rgb)
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
            prim           = gGpuPrimCursor;
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
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        }
    }
    SCRATCH_POP(RoomBillboardScratch);
}

/// Draws a radial glow around the screen position of `arg0`'s world
/// translation, unless the projection flags an error: sixteen overlapping
/// gouraud quads coloured `arg2` at the centre and black at the rim, radius
/// `arg1 * 64` over the OTZ plus one.
static void func_shelter_b1_main_corridor_8017F488(GfxCoord* arg0, s16 arg1, u8* arg2)
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
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        }
    }
    SCRATCH_POP_BYTES(0x18);
}

/// A coloured flash. Its first tick places it at the work block's position
/// under the block's parent, and splits `spawnArg1` into a duration (low half)
/// and a colour row (high half). For that many ticks it brightens and grows,
/// drawing a radial glow, a half-bright larger glow every other tick and a ring
/// that closes in; then it draws a starburst that grows as it dims by 0x10 a
/// tick, and releases its work block once it is dark. The colour is its
/// brightness shifted down per channel by the chosen row.
void func_shelter_b1_main_corridor_8017F81C(Task* arg0)
{
    u8          rgb[3];
    GpEffWork*  mem;
    GfxCoord*   coord;
    GpMtxWords* rot;
    s16         flag;
    s32         shift;

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
        switch (arg0->state) {
            case 0:
                rot                 = (GpMtxWords*)&coord->coord;
                coord->parent       = mem->parent;
                rot->m00_m01        = 0x1000;
                rot->m02_m10        = 0;
                rot->m11_m12        = 0x1000;
                rot->m20_m21        = 0;
                rot->m22            = 0x1000;
                coord->coord.t[0]   = mem->pos.vx;
                coord->coord.t[1]   = mem->pos.vy;
                coord->coord.t[2]   = mem->pos.vz;
                coord->composeStamp = GRAPHICS_COORD_DIRTY;
                Gp_UpdateCoord(coord);
                shift                 = arg0->spawnArg1.halves.high;
                mem->index            = shift;
                arg0->spawnArg1.value = arg0->spawnArg1.halves.low;
                arg0->state           = 1;
                mem->step             = 0x100 / arg0->spawnArg1.value;
                return;
            case 1:
                Gp_UpdateCoord(coord);
                mem->scale            += mem->step;
                mem->angle            += mem->step;
                arg0->spawnArg1.value -= 1;
                rgb[0]                 = mem->scale >> D_shelter_b1_main_corridor_801831D4.entries[mem->index].r;
                rgb[1]                 = mem->scale >> D_shelter_b1_main_corridor_801831D4.entries[mem->index].g;
                rgb[2]                 = mem->scale >> D_shelter_b1_main_corridor_801831D4.entries[mem->index].b;
                func_shelter_b1_main_corridor_8017F488(coord, mem->angle, rgb);
                rgb[0] = rgb[0] >> 1;
                rgb[1] = rgb[1] >> 1;
                rgb[2] = rgb[2] >> 1;
                if (mem->age & 1) {
                    func_shelter_b1_main_corridor_8017F488(coord, mem->angle + 0x100, rgb);
                }
                func_shelter_b1_main_corridor_8017F064(coord, (s16)(0x300 - (u16)mem->angle * 2), 0x80, rgb);
                if (arg0->spawnArg1.value == 0) {
                    mem->scale  = 0xFF;
                    arg0->state = 2;
                    return;
                }
                return;
            case 2:
                Gp_UpdateCoord(coord);
                if (mem->scale >= 0x11) {
                    rgb[0] = mem->scale >> D_shelter_b1_main_corridor_801831D4.entries[mem->index].r;
                    rgb[1] = mem->scale >> D_shelter_b1_main_corridor_801831D4.entries[mem->index].g;
                    rgb[2] = mem->scale >> D_shelter_b1_main_corridor_801831D4.entries[mem->index].b;
                    func_shelter_b1_main_corridor_80180604(coord, (u16)mem->angle * 4, rgb);
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

/// An orange burst: each tick draws a radial glow and a textured glow that
/// grow by 0x10 a tick, and while its ring colour lasts, a ring that widens by
/// 0x30 a tick as that colour dims by 0x18. Once the ring is gone the radial
/// glow dims by 0x18 a tick and the work block is released when it is dark.
void func_shelter_b1_main_corridor_8017FBB4(Task* arg0)
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
        func_shelter_b1_main_corridor_8017F488(coord, step * 2, rgb);
        func_shelter_b1_main_corridor_8017FD60(coord, mem->angle);
        if (mem->period >= 0x19) {
            rgb[0] = mem->period;
            rgb[1] = mem->period >> 1;
            rgb[2] = mem->period >> 2;
            func_shelter_b1_main_corridor_8017F064(coord, (s16)(mem->step * 3 / 2), 0x60, rgb);
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
static void func_shelter_b1_main_corridor_8017FD60(GfxCoord* coord, s16 size)
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
            func_shelter_b1_main_corridor_8018028C(&ground, outerSize);
        }
    }
    SCRATCH_POP(GpRingScratch);
}

/// Queues one semi-transparent textured quad lying flat at the coordinate's
/// world position: the unit quad `D_80111E38` is scaled by `arg1`, turned by
/// the view matrix and projected through `GsWSMATRIX`. Unless the GTE flags
/// the projection, the quad is coloured (0x30, 0x20, 0x20) and its texture
/// alternates between two 32-pixel columns on successive frames.
static void func_shelter_b1_main_corridor_8018028C(GfxCoord* arg0, s32 arg1)
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

/// Draws a starburst around the screen position of `arg0`'s world
/// translation, unless the projection flags an error: a radial glow of radius
/// `arg1 * 64` over the OTZ plus one at half of `arg2`, a brighter core of half
/// that radius at full `arg2`, and pointed rays between them. Every quad is
/// black at its rim.
static void func_shelter_b1_main_corridor_80180604(GfxCoord* arg0, s16 arg1, u8* arg2)
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

/// Emitter that lives 20 ticks: each tick it turns its heading by a random
/// 0x200-0x3FF and spawns effect `D_80115728` at its coordinate, moving 0x300
/// along that heading in XZ and -0x80 per tick of age in y. It releases its
/// work block at the end, or when the room's event state reaches 4.
void func_shelter_b1_main_corridor_80180FC4(Task* arg0)
{
    GpEffWork* mem;
    GfxCoord*  coord;
    s16        flag;
    s16        ang;

    mem   = arg0->spawnArg2.pointer;
    flag  = Gp_State1C->eventState;
    coord = arg0->extra.coordBody->coord;
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
void func_shelter_b1_main_corridor_801810F8(Task* arg0)
{
    GpEffWork* mem;
    GfxCoord*  coord;
    u8         rgb[3];

    mem   = arg0->spawnArg2.pointer;
    coord = arg0->extra.coordBody->coord;
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
                func_shelter_b1_main_corridor_801817C8(coord, mem->angle, rgb);
                rgb[0] >>= 1;
                rgb[1] >>= 1;
                rgb[2] >>= 1;
                func_shelter_b1_main_corridor_801817C8(coord, (u16)mem->angle * 2, rgb);
                func_shelter_b1_main_corridor_8018139C(coord, (s16)(0x300 - (u16)mem->angle * 2), 0x80, rgb);
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
                    func_shelter_b1_main_corridor_801826CC(coord, mem->angle * 3, rgb);
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

/// The same ring as `func_shelter_b1_main_corridor_8017F064`, compiled with a
/// different scratch-block layout: sixteen gouraud quads around the screen
/// position of `arg0`'s world translation, black at radius `(s16)arg1 * 64`
/// over the OTZ and `rgb` at `(s16)(arg1 + arg2) * 64` over it.
static void func_shelter_b1_main_corridor_8018139C(GfxCoord* arg0, s32 arg1, s32 arg2, u8* rgb)
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

/// A second copy of `func_shelter_b1_main_corridor_8017F488`, drawing the same
/// shape; the room links both.
static void func_shelter_b1_main_corridor_801817C8(GfxCoord* arg0, s16 arg1, u8* arg2)
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
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        }
    }
    SCRATCH_POP_BYTES(0x18);
}

/// A beam between two points under the work block's parent. Its first tick
/// allocates sixteen coordinates, two eight-slot trails, and fills both with
/// the points' current world matrices; each later tick records the points into
/// the next slot of each trail and draws the beam between the trails with
/// colour code 0x123. It releases its work block after `spawnArg1` ticks, and
/// does nothing once the room's event state reaches 2.
void func_shelter_b1_main_corridor_80181B5C(Task* task)
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
                objCoord->coord.t[0]   = D_shelter_b1_main_corridor_801831E8[0].vx;
                objCoord->coord.t[1]   = D_shelter_b1_main_corridor_801831E8[0].vy;
                objCoord->coord.t[2]   = D_shelter_b1_main_corridor_801831E8[0].vz;
                objCoord->composeStamp = GRAPHICS_COORD_DIRTY;
                Gp_UpdateCoord(objCoord);
                task->state        = 1;
                coord.parent       = work->parent;
                vec                = &D_shelter_b1_main_corridor_801831E8[1];
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
                    SVECTOR* edge    = &D_shelter_b1_main_corridor_801831E8[1];
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
                func_shelter_b1_main_corridor_8018204C(coords, &coords[8], work->age & 7, 0x123);
                if (work->age == task->spawnArg1.value && work->age != 0) {
                    Gp_ReleaseState1CMem(work, task);
                }
                break;
        }
    }
}

/// Draws the band between two eight-slot coordinate trails `arg0` and `arg1`
/// as seven gouraud quads, walking back from slot `arg2`; each quad joins the
/// translations of two adjacent slots of both trails. Brightness falls from
/// 0x40 by 9 per slot, multiplied per channel by the colour code `arg3` (red
/// from bits 8 up, green from bits 4-5, blue from bits 0-1). Quads the
/// projection flags as erroneous are skipped.
static void func_shelter_b1_main_corridor_8018204C(GfxCoord* arg0, GfxCoord* arg1, s16 arg2, s16 arg3)
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
        blk->v[0].vx = (u16)a->workm.t[0];
        j            = j - 1;
        blk->v[0].vy = (u16)a->workm.t[1];
        i1           = j & 7;
        blk->v[0].vz = (u16)a->workm.t[2];
        b            = &arg1[i0];
        blk->v[1].vx = (u16)b->workm.t[0];
        blk->v[1].vy = (u16)b->workm.t[1];
        blk->v[1].vz = (u16)b->workm.t[2];
        a            = &arg0[i1];
        blk->v[2].vx = (u16)a->workm.t[0];
        blk->v[2].vy = (u16)a->workm.t[1];
        blk->v[2].vz = (u16)a->workm.t[2];
        b            = &arg1[i1];
        blk->v[3].vx = (u16)b->workm.t[0];
        blk->v[3].vy = (u16)b->workm.t[1];
        blk->v[3].vz = (u16)b->workm.t[2];
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

/// A burst at the effect's coordinate: spawns effect 0x60076, then either
/// (non-zero `spawnArg1`) sprays effect 0x60070 in random directions for six
/// ticks, or spawns two 0x6007C effects and draws two orange rings, one fixed
/// and one widening by 0x30 a tick as they dim. It releases its work block
/// after seven ticks, or when the room's event state reaches 4.
void func_shelter_b1_main_corridor_80182444(Task* task)
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
            func_shelter_b1_main_corridor_8018139C(objCoord, 0x100, 0x100, rgb);
            func_shelter_b1_main_corridor_8018139C(objCoord, work->scale, work->scale, rgb);
            if (work->age >= 7) {
                task->state = 3;
            }
            break;

        case 3:
            Gp_ReleaseState1CMem(work, task);
            break;
    }
}

/// A second copy of `func_shelter_b1_main_corridor_80180604`, drawing the same
/// shape; the room links both.
static void func_shelter_b1_main_corridor_801826CC(GfxCoord* arg0, s16 arg1, u8* arg2)
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
