#include "rooms/dryfield_water_hole.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/abs.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "common.h"
#include "gte.h"

#include "actors/task_tables.h"

#include "gameplay/display.h"
#include "gameplay/actor_render.h"
#include "gameplay/area.h"
#include "gameplay/captions.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/direction_input.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/effects.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/light.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_combat.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/random.h"
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

#include "mapui/map_dryfield.h"

#include "rooms/room_common.h"
#include "../../shared/water_effects.h"
#include "../../shared/glow_draw.h"

#define D_dryfield_water_hole_8017FCC4 (D_dryfield_water_hole_8017FCBC + 1)
#define D_dryfield_water_hole_8017FCDC (D_dryfield_water_hole_8017FCBC + 4)
#define D_dryfield_water_hole_8017FD04 (D_dryfield_water_hole_8017FCBC + 9)

// One live spotlight is followed by retained exporter data in whole
// spotlight-sized slots. Its original role is unresolved; keep the bytes
// without treating stale pointer-looking words as live C pointers.
typedef struct {
    WorldCoordSpotLight active[1];
    u8                  retained[540];
} DryfieldWaterHoleSpotLightStorage;
STATIC_ASSERT_SIZEOF(DryfieldWaterHoleSpotLightStorage, 648);

/// One rectangle of water surface drawn by `func_dryfield_water_hole_8017D898`,
/// in world coordinates: it spans `width` along X from `x` and `depth` along Z
/// from `z`, at height `y`. The table ends at the first entry whose `y` word is
/// -1; the drawing code reads only its low half as the height.
typedef struct {
    s16 x;
    s16 z;
    s16 width;
    u16 depth;
    s32 y;
} _DryfieldWaterHoleSurface;

/// Block the room's splash task receives as `spawnArg2`. Only the halfword at
/// 0x26 is touched: an effect strength, set from how far a tracked part moved
/// this frame and used as the odds of spawning each of the two effects.
typedef struct {
    byte pad_0[0x26];
    s16  strength;
} _DryfieldWaterHoleSplash;

/// The room's message table, the `GpMsgEntry` list the room task publishes in
/// `Task::msgTable` for `Gp_DispatchMsg` to walk: 0x13EE, 0x13F1, 0x13EF, 0x13F0
/// and 0x13F2.
extern GpMsgEntry D_dryfield_water_hole_8017FC5C[];
/// Descriptor of the room's water task, spawned by the room task's entry tick.
/// Its callback is `func_dryfield_water_hole_8017DFA0`.
extern TaskDesc D_dryfield_water_hole_8017FC8C[];
/// The room's water surfaces, terminated by an entry with `y == -1`.
extern _DryfieldWaterHoleSurface D_dryfield_water_hole_8017FC98[];
/// Point pairs of the glowing beams the splash task draws, one table per group
/// of views.
/// Last-frame world positions of the two tracked parts of the slot-3 task's
/// model, compared against this frame's to measure how far each moved.
extern SVECTOR D_dryfield_water_hole_8017FD1C[];
/// Cursor into the primitive area the room's water surface is written to,
/// reset each frame to the half of that area belonging to the ordering table
/// being built.
extern u8* D_dryfield_water_hole_801828CC;
/// Frame counter the water surface's wave is phased by.
extern s16 D_dryfield_water_hole_801828D0;

static void func_dryfield_water_hole_8017E000(Task* arg0);

// Indexed views below share one contiguous table.
extern GpGridParams               D_dryfield_water_hole_80180260[1];
extern GpObj3A                    D_dryfield_water_hole_80181F28[2];
extern GpObj4C                    D_dryfield_water_hole_80181724[14];
extern GpObj4C                    D_dryfield_water_hole_80181B4C[7];
extern GpObj4C                    D_dryfield_water_hole_80181D60[6];
extern WorldCoordRoomAmbientEntry D_dryfield_water_hole_80182824[9];
extern GpRoomCoordSet             D_dryfield_water_hole_80182468[1];
extern GpRoomCoordSet             D_dryfield_water_hole_8018278C[1];

s32  func_dryfield_water_hole_8017D5E8(Task*, s32, TaskMessageArg, TaskMessageArg);
s32  func_dryfield_water_hole_8017D5F0(Task*, s32, RoomEventMsg*, RoomEventMsg*);
s32  func_dryfield_water_hole_8017D73C(Task*, s32, s32, s32);
s32  func_dryfield_water_hole_8017D784(Task*, s32, TaskMessageArg, TaskMessageArg);
s32  func_dryfield_water_hole_8017D78C(Task*, s32, s32, s32);
void func_dryfield_water_hole_8017DFA0(Task*);

extern DryfieldWaterHoleSpotLightStorage D_dryfield_water_hole_801821E0;
extern WorldCoordPointLight              D_dryfield_water_hole_80181FA0[6];

GpMsgEntry D_dryfield_water_hole_8017FC5C[6] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, func_dryfield_water_hole_8017D5F0 },
    { 5105, func_dryfield_water_hole_8017D5E8 },
    { 5103, func_dryfield_water_hole_8017D784 },
    { 5104, func_dryfield_water_hole_8017D73C },
    { 5106, func_dryfield_water_hole_8017D78C },
    { 0x7FFFFFFF, NULL },
};

TaskDesc D_dryfield_water_hole_8017FC8C[1] = {
    { { { TASK_BODY_NONE, 192 } }, func_dryfield_water_hole_8017DFA0, { .value = 0 } },
};

_DryfieldWaterHoleSurface D_dryfield_water_hole_8017FC98[3] = {
    { 4000, -2000, 8000, 2000, -420 },
    { 0x2710, -4000, 0x32C8, 2000, -420 },
    { 0, 0, 0, 0, -1 },
};

SVECTOR D_dryfield_water_hole_8017FCBC[12] = {
    { 9500, -1835, -100, 0 },
    { 10500, -1835, -100, 0 },
    { 9500, -1750, -75, 0 },
    { 10500, -1750, -75, 0 },
    { 14300, -1835, -3900, 0 },
    { 15300, -1835, -3900, 0 },
    { 14300, -1750, -3925, 0 },
    { 15300, -1750, -3925, 0 },
    { 18300, -1835, -2120, 0 },
    { 19300, -1835, -2120, 0 },
    { 18300, -1750, -2095, 0 },
    { 19300, -1750, -2095, 0 },
};

SVECTOR D_dryfield_water_hole_8017FD1C[2] = { 0 };

GpRoomObjRec D_dryfield_water_hole_8017FD2C[4] = {
    { D_dryfield_water_hole_80180260, D_dryfield_water_hole_80181724, D_dryfield_water_hole_80181B4C, D_dryfield_water_hole_80181F28 },
    { D_dryfield_water_hole_80180260, D_dryfield_water_hole_80181724, D_dryfield_water_hole_80181D60, D_dryfield_water_hole_80181F28 },
    { D_dryfield_water_hole_80180260, D_dryfield_water_hole_80181724, D_dryfield_water_hole_80181B4C, D_dryfield_water_hole_80181F28 },
    { D_dryfield_water_hole_80180260, D_dryfield_water_hole_80181724, D_dryfield_water_hole_80181D60, D_dryfield_water_hole_80181F28 },
};

u8 D_dryfield_water_hole_8017FD6C[8] = {
    1,
    12,
    13,
    14,
    15,
    16,
    17,
    18,
};

u8 D_dryfield_water_hole_8017FD74[8] = {
    1,
    2,
    3,
    4,
    5,
    6,
    10,
    9,
};

u8 D_dryfield_water_hole_8017FD7C[8] = {
    1,
    12,
    13,
    14,
    15,
    16,
    20,
    19,
};

u8* D_dryfield_water_hole_8017FD84[4] = {
    D_8010CAF8,
    D_dryfield_water_hole_8017FD6C,
    D_dryfield_water_hole_8017FD74,
    D_dryfield_water_hole_8017FD7C,
};

GpViewCountRec D_dryfield_water_hole_8017FD94[4] = {
    { { .bytes = { 8, 0 } } },
    { { .bytes = { 8, 0 } } },
    { { .bytes = { 8, 0 } } },
    { { .bytes = { 8, 0 } } },
};

GpRoomCoordRec D_dryfield_water_hole_8017FD9C[4] = {
    { D_dryfield_water_hole_80182468, D_dryfield_water_hole_80182824 },
    { D_dryfield_water_hole_8018278C, NULL },
    { D_dryfield_water_hole_80182468, D_dryfield_water_hole_80182824 },
    { D_dryfield_water_hole_8018278C, NULL },
};

GpWarpRec D_dryfield_water_hole_8017FDBC[3] = {
    { { .words = { 0, 7400, 0, -1350 } }, { 0, 0, 0, 0 }, { .words = { 0, 7400, 0, -1350 } }, { 0, 0, 0, 0 }, 0x52200003, 0x52200003, 0, 3, 2, 0 },
    { { .words = { 3072, 0x57C0, -534, -3076 } }, { 0, 0, 0, 0 }, { .words = { 3072, 0x57C0, -534, -3076 } }, { 0, 0, 0, 0 }, 0x52200002, 0x52200001, 0, 8, 0, 462 },
    { { .words = { 1024, 4680, 0, -954 } }, { 0, 0, 0, 0 }, { .words = { 1024, 4680, 0, -954 } }, { 0, 0, 0, 0 }, 0, 0, 0, 2, 0, 445 },
};

SVECTOR D_dryfield_water_hole_8017FE64[10] = {
#include "assets/dryfield_water_hole_collision_02CA0_normals.inc"
};

SVECTOR D_dryfield_water_hole_8017FEB4[54] = {
#include "assets/dryfield_water_hole_collision_02CA0_verts.inc"
};

WorldCollisionGridFace D_dryfield_water_hole_80180064[23] = {
#include "assets/dryfield_water_hole_collision_02CA0_faces.inc"
};

s16 D_dryfield_water_hole_80180178[92] = {
#include "assets/dryfield_water_hole_collision_02CA0_cells.inc"
};

#define GRID_CELL(i) (&D_dryfield_water_hole_80180178[i])
s16* D_dryfield_water_hole_80180230[12] = {
#include "assets/dryfield_water_hole_collision_02CA0_table.inc"
};
#undef GRID_CELL

GpGridParams D_dryfield_water_hole_80180260[1] = {
    { NULL, D_dryfield_water_hole_8017FE64, D_dryfield_water_hole_8017FEB4, D_dryfield_water_hole_80180064, D_dryfield_water_hole_80180230, -4000, 5000, 6, 2, 4000, 23 },
};

GpViewRec D_dryfield_water_hole_80180284[26] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { -0x34B7, 0x7530, 3935 } }, 329 },
    { { { { -1708, 0, 3722 }, { 704, 4022, 323 }, { -3655, 774, -1677 } }, { -7853, 1372, 107 } }, 230 },
    { { { { -1222, 0, -3909 }, { -357, 4078, 111 }, { 3892, 374, -1217 } }, { -4081, 1167, 312 } }, 230 },
    { { { { -1694, 0, -3728 }, { -261, 4085, 118 }, { 3719, 287, -1690 } }, { -7539, 1082, 155 } }, 243 },
    { { { { -3680, 0, -1797 }, { -224, 4063, 459 }, { 1783, 511, -3651 } }, { -0x2893, 1214, 35 } }, 207 },
    { { { { -713, 0, 4033 }, { 463, 4068, 82 }, { -4006, 471, -708 } }, { -0x497F, 1141, 2467 } }, 230 },
    { { { { -856, 0, -4005 }, { -160, 4092, 34 }, { 4002, 163, -855 } }, { -0x3AA3, 1023, 2449 } }, 230 },
    { { { { -1174, 0, -3924 }, { 17, 4095, -5 }, { 3924, -18, -1174 } }, { -0x4AF8, 1095, 2331 } }, 230 },
    { { { { -1174, 0, -3924 }, { 17, 4095, -5 }, { 3924, -18, -1174 } }, { -0x4AF8, 1095, 2331 } }, 230 },
    { { { { -856, 0, -4005 }, { -160, 4092, 34 }, { 4002, 163, -855 } }, { -0x3AA3, 1023, 2449 } }, 230 },
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { -0x34B7, 0x7530, 3935 } }, 329 },
    { { { { -1708, 0, 3722 }, { 704, 4022, 323 }, { -3655, 774, -1677 } }, { -7853, 1372, 107 } }, 230 },
    { { { { -1222, 0, -3909 }, { -357, 4078, 111 }, { 3892, 374, -1217 } }, { -4081, 1167, 312 } }, 230 },
    { { { { -1694, 0, -3728 }, { -261, 4085, 118 }, { 3719, 287, -1690 } }, { -7539, 1082, 155 } }, 243 },
    { { { { -3680, 0, -1797 }, { -224, 4063, 459 }, { 1783, 511, -3651 } }, { -0x2893, 1214, 35 } }, 207 },
    { { { { -713, 0, 4033 }, { 463, 4068, 82 }, { -4006, 471, -708 } }, { -0x497F, 1141, 2467 } }, 230 },
    { { { { -856, 0, -4005 }, { -160, 4092, 34 }, { 4002, 163, -855 } }, { -0x3AA3, 1023, 2449 } }, 230 },
    { { { { -1174, 0, -3924 }, { 17, 4095, -5 }, { 3924, -18, -1174 } }, { -0x4AF8, 1095, 2331 } }, 230 },
    { { { { -1174, 0, -3924 }, { 17, 4095, -5 }, { 3924, -18, -1174 } }, { -0x4AF8, 1095, 2331 } }, 230 },
    { { { { -856, 0, -4005 }, { -160, 4092, 34 }, { 4002, 163, -855 } }, { -0x3AA3, 1023, 2449 } }, 230 },
    { { { { -3186, 0, -2573 }, { -25, 4095, 31 }, { 2573, 40, -3186 } }, { -8630, 1300, -20 } }, 246 },
    { { { { -2620, 0, 3148 }, { 219, 4086, 182 }, { -3140, 285, -2613 } }, { -9960, 1360, 430 } }, 246 },
    { { { { -2395, 0, -3322 }, { -237, 4085, 171 }, { 3313, 293, -2389 } }, { -9120, 1360, 430 } }, 246 },
    { { { { -3186, 0, -2573 }, { -25, 4095, 31 }, { 2573, 40, -3186 } }, { -8630, 1300, -20 } }, 246 },
    { { { { -2620, 0, 3148 }, { 219, 4086, 182 }, { -3140, 285, -2613 } }, { -9960, 1360, 430 } }, 246 },
    { { { { -2395, 0, -3322 }, { -237, 4085, 171 }, { 3313, 293, -2389 } }, { -9120, 1360, 430 } }, 246 },
};

SpriteBatch D_dryfield_water_hole_8018062C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_water_hole_8018063C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_water_hole_8018064C[21] = {
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -88, -24, 2000, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -80, -72, 2000, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -32, -72, 2000, { .fields = { 88, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -16, -72, 2000, { .fields = { 72, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 0, -72, 2000, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -56, -72, 2000, { .fields = { 88, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -56, -24, 2000, { .fields = { 64, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -32, -24, 2000, { .fields = { 56, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -24, -72, 1375, { .fields = { 56, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -48, -72, 1375, { .fields = { 48, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 8, -72, 1375, { .fields = { 104, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 8, -24, 1375, { .fields = { 104, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, 32, -24, 1375, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, 32, -72, 1375, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -8, -24, 1375, { .fields = { 112, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -8, -72, 1375, { .fields = { 112, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -16, -120, 875, { .fields = { 72, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 0, -120, 875, { .fields = { 64, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 16, -120, 875, { .fields = { 64, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 16, -80, 875, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 0, -80, 875, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_water_hole_801807F0[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 8, 0, 0, { 0, 0 } },
    { 8, 8, 0, 0, { 2, 0 } },
    { 16, 5, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_water_hole_80180818[33] = {
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 112, -120, 625, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 112, -72, 625, { .fields = { 32, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 112, -24, 625, { .fields = { 32, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 112, 24, 625, { .fields = { 80, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, 72, -120, 625, { .fields = { 88, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 48, -120, 625, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 32, -120, 625, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 16, -120, 625, { .fields = { 80, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 0, -120, 625, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, 72, -72, 625, { .fields = { 32, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, 72, -24, 625, { .fields = { 72, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, 72, 24, 625, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 48, -72, 625, { .fields = { 64, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 48, -24, 625, { .fields = { 40, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 48, 24, 625, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, -120, -120, 1250, { .fields = { 120, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, -120, -72, 1250, { .fields = { 112, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, -120, -24, 1250, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, -72, -120, 1250, { .fields = { 112, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, -72, -72, 1250, { .fields = { 64, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, -72, -24, 1250, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 24 } }, -72, 24, 1250, { .fields = { 56, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -24, -120, 1250, { .fields = { 104, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -24, -72, 1250, { .fields = { 112, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -24, -24, 1250, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 24 } }, -120, 24, 1250, { .fields = { 48, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -24, 24, 1250, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -8, -120, 1250, { .fields = { 96, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 8, -120, 1250, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -8, -72, 1250, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, 24, -112, 1250, { .fields = { 88, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 40, -112, 1250, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, 56, -112, 1250, { .fields = { 48, 240 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_water_hole_80180AAC[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 15, 0, 0, { 1, 0 } },
    { 15, 18, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_water_hole_80180ACC[18] = {
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -160, -24, 625, { .fields = { 80, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -160, -72, 625, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 24 } }, -160, -96, 625, { .fields = { 8, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 32 } }, -160, 24, 625, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -112, 24, 625, { .fields = { 48, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, -112, -24, 625, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, -112, -72, 625, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -112, -104, 625, { .fields = { 48, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -80, -112, 625, { .fields = { 56, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -80, -72, 625, { .fields = { 104, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -80, -24, 625, { .fields = { 80, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -80, 24, 625, { .fields = { 56, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -56, 24, 625, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -56, -24, 625, { .fields = { 112, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -56, -72, 625, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -40, -120, 625, { .fields = { 48, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -24, -120, 625, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -56, -112, 625, { .fields = { 64, 128 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_water_hole_80180C34[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 18, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_water_hole_80180C4C[19] = {
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 56, -72, 1375, { .fields = { 72, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 72, -32, 1375, { .fields = { 112, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 72, -72, 1375, { .fields = { 72, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 88, -72, 1375, { .fields = { 72, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 112, -32, 1375, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 112, -72, 1375, { .fields = { 64, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 32, -72, 1375, { .fields = { 64, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 56, -32, 1375, { .fields = { 112, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 88, -32, 1375, { .fields = { 104, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 40, -32, 2000, { .fields = { 88, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 40, -72, 2000, { .fields = { 104, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 56, -72, 2000, { .fields = { 88, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 72, -72, 2000, { .fields = { 64, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 24, -32, 2000, { .fields = { 96, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 24, -72, 2000, { .fields = { 88, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 0, -72, 2000, { .fields = { 88, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 0, -32, 2000, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 56, -64, 3000, { .fields = { 120, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 48, -16, 3000, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_water_hole_80180DC8[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 9, 0, 0, { 1, 0 } },
    { 9, 8, 0, 0, { 2, 0 } },
    { 17, 2, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_water_hole_80180DF0[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_water_hole_80180E00[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_water_hole_80180E10[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_water_hole_80180E20[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_water_hole_80180E30[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_water_hole_80180E40[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_water_hole_80180E50[21] = {
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -8, -72, 1375, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -24, -72, 1375, { .fields = { 56, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -48, -72, 1375, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 8, -24, 1375, { .fields = { 104, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, 32, -24, 1375, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -8, -24, 1375, { .fields = { 112, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, 32, -72, 1375, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 8, -72, 1375, { .fields = { 88, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -16, -72, 2000, { .fields = { 72, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 0, -72, 2000, { .fields = { 56, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -32, -72, 2000, { .fields = { 88, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -32, -24, 2000, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -88, -24, 2000, { .fields = { 48, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -56, -72, 2000, { .fields = { 104, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -56, -24, 2000, { .fields = { 56, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -80, -72, 2000, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -16, -120, 875, { .fields = { 72, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 0, -120, 875, { .fields = { 72, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 16, -120, 875, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 16, -80, 875, { .fields = { 56, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 0, -80, 875, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_water_hole_80180FF4[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 8, 0, 0, { 2, 0 } },
    { 8, 8, 0, 0, { 0, 0 } },
    { 16, 5, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_water_hole_8018101C[33] = {
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 112, 24, 625, { .fields = { 80, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 112, -24, 625, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 112, -72, 625, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 16, -120, 625, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 0, -120, 625, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 112, -120, 625, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 32, -120, 625, { .fields = { 112, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, 72, -120, 625, { .fields = { 72, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 48, -120, 625, { .fields = { 56, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, 72, -72, 625, { .fields = { 40, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, 72, -24, 625, { .fields = { 40, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, 72, 24, 625, { .fields = { 32, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 48, 24, 625, { .fields = { 32, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 48, -24, 625, { .fields = { 16, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 48, -72, 625, { .fields = { 16, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, -120, -120, 1250, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, -72, -120, 1250, { .fields = { 112, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -24, -120, 1250, { .fields = { 16, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -8, -120, 1250, { .fields = { 0, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, 8, -120, 1250, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, 24, -112, 1250, { .fields = { 96, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 40, -112, 1250, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, 56, -112, 1250, { .fields = { 72, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, -120, -72, 1250, { .fields = { 96, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, -120, -24, 1250, { .fields = { 80, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 24 } }, -120, 24, 1250, { .fields = { 48, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -72, -72, 1250, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, -72, -24, 1250, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 24 } }, -72, 24, 1250, { .fields = { 48, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -24, 24, 1250, { .fields = { 48, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -24, -24, 1250, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -24, -72, 1250, { .fields = { 0, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -8, -72, 1250, { .fields = { 80, 168 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_water_hole_801812B0[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 15, 0, 0, { 1, 0 } },
    { 15, 18, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_water_hole_801812D0[18] = {
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -56, 24, 625, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -56, -24, 625, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -56, -72, 625, { .fields = { 80, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -40, -120, 625, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -80, 24, 625, { .fields = { 56, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -80, -24, 625, { .fields = { 80, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -80, -72, 625, { .fields = { 104, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -80, -112, 625, { .fields = { 56, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 32 } }, -160, 24, 625, { .fields = { 16, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -160, -24, 625, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -160, -72, 625, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 24 } }, -160, -96, 625, { .fields = { 8, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -112, 24, 625, { .fields = { 48, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, -112, -24, 625, { .fields = { 96, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, -112, -72, 625, { .fields = { 96, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -112, -104, 625, { .fields = { 48, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -24, -120, 625, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -56, -112, 625, { .fields = { 64, 168 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_water_hole_80181438[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 18, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_water_hole_80181450[19] = {
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 40, -32, 2000, { .fields = { 72, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 40, -72, 2000, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 56, -72, 2000, { .fields = { 96, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 72, -72, 2000, { .fields = { 72, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 24, -72, 2000, { .fields = { 88, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 24, -32, 2000, { .fields = { 88, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 0, -32, 2000, { .fields = { 64, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 0, -72, 2000, { .fields = { 64, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 56, -32, 1375, { .fields = { 112, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 56, -72, 1375, { .fields = { 104, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 72, -32, 1375, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 88, -32, 1375, { .fields = { 104, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 112, -32, 1375, { .fields = { 104, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 112, -72, 1375, { .fields = { 80, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 88, -72, 1375, { .fields = { 88, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 72, -72, 1375, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 32, -72, 1375, { .fields = { 72, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 48, -16, 3000, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 56, -64, 3000, { .fields = { 120, 144 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_water_hole_801815CC[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 8, 0, 0, { 1, 0 } },
    { 8, 9, 0, 0, { 2, 0 } },
    { 17, 2, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_water_hole_801815F4[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_water_hole_80181604[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_water_hole_80181614[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_water_hole_80181624[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

GpSprtRec D_dryfield_water_hole_80181634[20] = {
    { { .empty = D_dryfield_water_hole_8018062C }, D_dryfield_water_hole_8018062C, NULL },
    { { .empty = D_dryfield_water_hole_8018063C }, D_dryfield_water_hole_8018063C, NULL },
    { { .elements = D_dryfield_water_hole_8018064C }, D_dryfield_water_hole_801807F0, NULL },
    { { .elements = D_dryfield_water_hole_80180818 }, D_dryfield_water_hole_80180AAC, NULL },
    { { .elements = D_dryfield_water_hole_80180ACC }, D_dryfield_water_hole_80180C34, NULL },
    { { .elements = D_dryfield_water_hole_80180C4C }, D_dryfield_water_hole_80180DC8, NULL },
    { { .empty = D_dryfield_water_hole_80180DF0 }, D_dryfield_water_hole_80180DF0, NULL },
    { { .empty = D_dryfield_water_hole_80180E00 }, D_dryfield_water_hole_80180E00, NULL },
    { { .empty = D_dryfield_water_hole_80180E10 }, D_dryfield_water_hole_80180E10, NULL },
    { { .empty = D_dryfield_water_hole_80180E20 }, D_dryfield_water_hole_80180E20, NULL },
    { { .empty = D_dryfield_water_hole_80180E30 }, D_dryfield_water_hole_80180E30, NULL },
    { { .empty = D_dryfield_water_hole_80180E40 }, D_dryfield_water_hole_80180E40, NULL },
    { { .elements = D_dryfield_water_hole_80180E50 }, D_dryfield_water_hole_80180FF4, NULL },
    { { .elements = D_dryfield_water_hole_8018101C }, D_dryfield_water_hole_801812B0, NULL },
    { { .elements = D_dryfield_water_hole_801812D0 }, D_dryfield_water_hole_80181438, NULL },
    { { .elements = D_dryfield_water_hole_80181450 }, D_dryfield_water_hole_801815CC, NULL },
    { { .empty = D_dryfield_water_hole_801815F4 }, D_dryfield_water_hole_801815F4, NULL },
    { { .empty = D_dryfield_water_hole_80181604 }, D_dryfield_water_hole_80181604, NULL },
    { { .empty = D_dryfield_water_hole_80181614 }, D_dryfield_water_hole_80181614, NULL },
    { { .empty = D_dryfield_water_hole_80181624 }, D_dryfield_water_hole_80181624, NULL },
};

GpObj4C D_dryfield_water_hole_80181724[14] = {
    { NULL, NULL, NULL, { 6197, -1152, -1034, 0 }, { { 150, -2176, -1012, 0 }, { -150, -2176, 1012, 0 }, { 150, 2176, -1012, 0 }, { -150, 2176, 1012, 0 } }, { 4053, 0, 599, 0 }, { 0, 0, 4096, 0 }, 2401, 0, 3, 2, 1, 0 },
    { NULL, NULL, NULL, { 6304, -1104, -1024, 0 }, { { -154, -2128, 1004, 0 }, { 144, -2128, -1022, 0 }, { -154, 2128, 1004, 0 }, { 144, 2128, -1022, 0 } }, { -4056, 0, -597, 0 }, { 0, 0, 4096, 0 }, 2360, 0, 2, 3, 1, 0 },
    { NULL, NULL, NULL, { 9467, -1248, -1028, 0 }, { { -345, -2272, -963, 0 }, { 345, -2272, 963, 0 }, { -345, 2272, -963, 0 }, { 345, 2272, 963, 0 } }, { 3869, 0, -1388, 0 }, { 0, 0, 4096, 0 }, 2482, 0, 4, 3, 1, 0 },
    { NULL, NULL, NULL, { 9632, -1200, -1026, 0 }, { { 340, -2224, 960, 0 }, { -350, -2224, -966, 0 }, { 340, 2224, 960, 0 }, { -350, 2224, -966, 0 } }, { -3865, 0, 1383, 0 }, { 0, 0, 4096, 0 }, 2442, 0, 3, 4, 1, 0 },
    { NULL, NULL, NULL, { 0x2A9F, -1088, -1922, 0 }, { { 919, -2112, 432, 0 }, { -929, -2112, -443, 0 }, { 919, 2112, 432, 0 }, { -929, 2112, -443, 0 } }, { -1765, 0, 3723, 0 }, { 0, 0, 4096, 0 }, 2346, 0, 4, 5, 1, 0 },
    { NULL, NULL, NULL, { 0x2A7E, -1168, -1858, 0 }, { { -927, -2192, -439, 0 }, { 923, -2192, 434, 0 }, { -927, 2192, -439, 0 }, { 923, 2192, 434, 0 } }, { 1748, 0, -3710, 0 }, { 0, 0, 4096, 0 }, 2415, 0, 5, 4, 1, 0 },
    { NULL, NULL, NULL, { 0x319F, -1248, -2945, 0 }, { { 435, -2272, -931, 0 }, { -441, -2272, 919, 0 }, { 435, 2272, -931, 0 }, { -441, 2272, 919, 0 } }, { 3717, 0, 1758, 0 }, { 0, 0, 4096, 0 }, 2482, 0, 6, 5, 1, 0 },
    { NULL, NULL, NULL, { 0x323F, -1136, -2976, 0 }, { { -441, -2160, 919, 0 }, { 435, -2160, -931, 0 }, { -441, 2160, 919, 0 }, { 435, 2160, -931, 0 } }, { -3706, 0, -1755, 0 }, { 0, 0, 4096, 0 }, 2387, 0, 5, 6, 1, 0 },
    { NULL, NULL, NULL, { 0x415F, -1200, -2912, 0 }, { { 198, -2224, 1001, 0 }, { -202, -2224, -1006, 0 }, { 198, 2224, 1001, 0 }, { -202, 2224, -1006, 0 } }, { -4027, 0, 801, 0 }, { 0, 0, 4096, 0 }, 2442, 0, 6, 7, 1, 0 },
    { NULL, NULL, NULL, { 0x40FF, -1328, -2977, 0 }, { { -205, -2352, -1008, 0 }, { 195, -2352, 999, 0 }, { -205, 2352, -1008, 0 }, { 195, 2352, 999, 0 } }, { 4021, 0, -803, 0 }, { 0, 0, 4096, 0 }, 2560, 0, 7, 6, 1, 0 },
    { NULL, NULL, NULL, { 0x531F, -1376, -2978, 0 }, { { 0, -2400, 1024, 0 }, { 0, -2400, -1023, 0 }, { 0, 2400, 1024, 0 }, { 0, 2400, -1023, 0 } }, { -4116, 0, 0, 0 }, { 0, 0, 4096, 0 }, 2598, 0, 7, 8, 1, 0 },
    { NULL, NULL, NULL, { 0x523F, -1408, -2978, 0 }, { { 51, -2432, -1022, 0 }, { -50, -2432, 1022, 0 }, { 51, 2432, -1022, 0 }, { -50, 2432, 1022, 0 } }, { 4093, 0, 200, 0 }, { 0, 0, 4096, 0 }, 2635, 0, 8, 7, 1, 0 },
    { NULL, NULL, NULL, { 0x5800, -1216, -6144, 0 }, { { 0, -2176, -1024, 0 }, { 0, -2176, 1024, 0 }, { 0, 2176, -1024, 0 }, { 0, 2176, 1024, 0 } }, { 4102, 0, 0, 0 }, { 0, 0, 4096, 0 }, 2401, 0, 9, 8, 1, 0 },
    { NULL, NULL, NULL, { 0x58C0, -1344, -6144, 0 }, { { 0, -2176, 1024, 0 }, { 0, -2176, -1024, 0 }, { 0, 2176, 1024, 0 }, { 0, 2176, -1024, 0 } }, { -4103, 0, 0, 0 }, { 0, 0, 4096, 0 }, 2401, 0, 8, 9, 129, 0 },
};

GpObj4C D_dryfield_water_hole_80181B4C[7] = {
    { NULL, NULL, NULL, { 7776, -48, -1584, 0 }, { { -671, 0, -336, 0 }, { 672, 0, -336, 0 }, { -671, 0, 336, 0 }, { 672, 0, 336, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, 4096, 0 }, 749, 0, 25, 18, 2, 0 },
    { NULL, NULL, NULL, { 0x5890, -1504, -2992, 0 }, { { 0, 1408, -1199, 0 }, { 0, 1408, 1200, 0 }, { 0, -1408, -1199, 0 }, { 0, -1408, 1200, 0 } }, { -4098, 0, 0, 0 }, { -4096, 0, 0, 0 }, 1846, 0x8000, 38, 34, 2, 0 },
    { NULL, NULL, NULL, { 0x5840, -416, -2944, 0 }, { { 288, 0, -671, 0 }, { 288, 0, 672, 0 }, { -288, 0, -671, 0 }, { -288, 0, 672, 0 } }, { 0, 4096, 0, 0 }, { 4096, 0, 0, 0 }, 729, 0x8101, 146, 192, 2, 0 },
    { NULL, NULL, NULL, { 0x5640, -32, -2976, 0 }, { { 288, 0, -671, 0 }, { 288, 0, 672, 0 }, { -288, 0, -671, 0 }, { -288, 0, 672, 0 } }, { 0, 4096, 0, 0 }, { -4096, 0, 0, 0 }, 729, 1, 149, 64, 2, 0 },
    { NULL, NULL, NULL, { 4384, -64, -976, 0 }, { { -415, 0, -1120, 0 }, { 416, 0, -1120, 0 }, { -415, 0, 1120, 0 }, { 416, 0, 1120, 0 } }, { 0, 4098, 0, 0 }, { 4091, 0, -201, 0 }, 1193, 2, 2, 255, 2, 0 },
    { NULL, NULL, NULL, { 7776, -64, -2208, 0 }, { { -1055, 0, 80, 0 }, { 1056, 0, 80, 0 }, { -1055, 0, 880, 0 }, { 1056, 0, 880, 0 } }, { 0, 4103, 0, 0 }, { 0, 0, 4096, 0 }, 1372, 0, 25, 18, 4, 0 },
    { NULL, NULL, NULL, { 0x4000, -64, -4352, 0 }, { { 960, 0, 225, 0 }, { 960, 0, 1312, 0 }, { -1152, 0, 225, 0 }, { -1152, 0, 1312, 0 } }, { 0, 4110, 0, 0 }, { -4096, 0, 0, 0 }, 1745, 2, 6, 0, 132, 0 },
};

GpObj4C D_dryfield_water_hole_80181D60[6] = {
    { NULL, NULL, NULL, { 7776, -48, -1584, 0 }, { { -671, 0, -336, 0 }, { 672, 0, -336, 0 }, { -671, 0, 336, 0 }, { 672, 0, 336, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, 4096, 0 }, 749, 0, 25, 18, 2, 0 },
    { NULL, NULL, NULL, { 0x5890, -1504, -2992, 0 }, { { 0, 1408, -1199, 0 }, { 0, 1408, 1200, 0 }, { 0, -1408, -1199, 0 }, { 0, -1408, 1200, 0 } }, { -4098, 0, 0, 0 }, { -4096, 0, 0, 0 }, 1846, 0x8000, 38, 34, 2, 0 },
    { NULL, NULL, NULL, { 0x5840, -416, -2944, 0 }, { { 288, 0, -671, 0 }, { 288, 0, 672, 0 }, { -288, 0, -671, 0 }, { -288, 0, 672, 0 } }, { 0, 4096, 0, 0 }, { 4096, 0, 0, 0 }, 729, 0x8101, 146, 192, 2, 0 },
    { NULL, NULL, NULL, { 0x5640, -32, -2976, 0 }, { { 288, 0, -671, 0 }, { 288, 0, 672, 0 }, { -288, 0, -671, 0 }, { -288, 0, 672, 0 } }, { 0, 4096, 0, 0 }, { -4096, 0, 0, 0 }, 729, 1, 149, 64, 2, 0 },
    { NULL, NULL, NULL, { 4384, -64, -976, 0 }, { { -415, 0, -1120, 0 }, { 416, 0, -1120, 0 }, { -415, 0, 1120, 0 }, { 416, 0, 1120, 0 } }, { 0, 4098, 0, 0 }, { 4091, 0, -201, 0 }, 1193, 2, 2, 255, 2, 0 },
    { NULL, NULL, NULL, { 7776, -64, -2208, 0 }, { { -1055, 0, 176, 0 }, { 1056, 0, 176, 0 }, { -1055, 0, 1040, 0 }, { 1056, 0, 1040, 0 } }, { 0, 4098, 0, 0 }, { 0, 0, 4096, 0 }, 1481, 0, 25, 18, 132, 0 },
};

GpObj3A D_dryfield_water_hole_80181F28[2] = {
    { NULL, NULL, { 8544, -1520, -3664, 0 }, { { -1536, -2224, -1840, 0 }, { -1536, 2224, -1840, 0 }, { 1536, -2224, 1840, 0 }, { 1536, 2224, 1840, 0 } }, { -3150, 0, 2629, 0 }, { -60, 12 }, 1, 0 },
    { NULL, NULL, { 0x34E0, -1568, -176, 0 }, { { -1504, -2224, -1936, 0 }, { -1504, 2224, -1936, 0 }, { 1504, -2224, 1936, 0 }, { 1504, 2224, 1936, 0 } }, { -3237, 0, 2514, 0 }, { -20, 12 }, 129, 0 },
};

WorldCoordPointLight D_dryfield_water_hole_80181FA0[6] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 9250, -1650, -600 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 3604, 2867 }, { 0, 0 } }, 1500, 2700 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x374A, -1650, -3400 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 3604, 2867 }, { 0, 0 } }, 1664, 3367 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x4A12, -1650, -2600 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3622, 3423, 3247 }, { 0, 0 } }, 1518, 3158 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x57C0, -4125, -2264 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 3440, 2457 }, { 0, 0 } }, 3000, 6000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 5000, -1290, -663 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1638, 1441, 1146 }, { 0, 0 } }, 1000, 2700 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x2B64, -1277, -2296 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1638, 1441, 1146 }, { 0, 0 } }, 1000, 2700 },
};

DryfieldWaterHoleSpotLightStorage D_dryfield_water_hole_801821E0 = {
    {
        { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { -4096, 0, 0 }, { 0, 0, 4096 }, { 0, 4096, 0 } }, { 7394, -3968, -1496 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 3604, 2867 }, { 0, 0 } }, { 0, 4096, 0, 0 }, 3000, 6000, 113 },
    },
    {
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x06,
        0x00,
        0x00,
        0x00,
        0x08,
        0x9A,
        0x18,
        0x80,
        0x01,
        0x00,
        0x00,
        0x00,
        0x48,
        0x9C,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0xB8,
        0x06,
        0xBC,
        0x08,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0xB8,
        0x0B,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0xB8,
        0x06,
        0x00,
        0x00,
        0x99,
        0x09,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0xB8,
        0x0B,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0xA2,
        0x48,
        0x00,
        0x00,
        0x8E,
        0xF9,
        0xFF,
        0xFF,
        0x48,
        0xF4,
        0xFF,
        0xFF,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0xB8,
        0x06,
        0xBC,
        0x08,
        0x99,
        0x09,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0xB8,
        0x0B,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0xE1,
        0x5F,
        0x00,
        0x00,
        0x92,
        0xEF,
        0xFF,
        0xFF,
        0xD8,
        0xF6,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0xCC,
        0x0C,
        0x3D,
        0x0A,
        0x66,
        0x06,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x40,
        0x1F,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x8E,
        0xF9,
        0xFF,
        0xFF,
        0x15,
        0xFC,
        0xFF,
        0xFF,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0xB8,
        0x0B,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x10,
        0x00,
        0x00,
        0x9C,
        0x2A,
        0x00,
        0x00,
        0x03,
        0xFB,
        0xFF,
        0xFF,
        0x08,
        0xF7,
        0xFF,
        0xFF,
    },
};

GpRoomCoordSet D_dryfield_water_hole_80182468[1] = {
    { 0, NULL, 6, D_dryfield_water_hole_80181FA0, 1, D_dryfield_water_hole_801821E0.active },
};

WorldCoordPointLight D_dryfield_water_hole_80182480[7] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 8650, -1650, -1100 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1720, 2236, 2457 }, { 0, 0 } }, 0, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x3A06, -1650, -3100 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1720, 2236, 2457 }, { 0, 0 } }, 0, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x48A2, -1650, -3000 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1720, 2236, 2457 }, { 0, 0 } }, 0, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x5FE1, -4206, -2344 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3276, 2621, 1638 }, { 0, 0 } }, 0, 8000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 5500, -1650, -1003 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1720, 2236, 2457 }, { 0, 0 } }, 0, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x2A9C, -1277, -2296 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1720, 2236, 2457 }, { 0, 0 } }, 0, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x5208, -1277, -2996 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1720, 2236, 2457 }, { 0, 0 } }, 0, 3000 },
};

WorldCoordSpotLight D_dryfield_water_hole_80182720[1] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 7444, -3447, -1496 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3276, 3014, 2621 }, { 0, 0 } }, { 0, 4096, 0, 0 }, 2500, 5000, 113 },
};

GpRoomCoordSet D_dryfield_water_hole_8018278C[1] = {
    { 0, NULL, 7, D_dryfield_water_hole_80182480, 1, D_dryfield_water_hole_80182720 },
};

GpAreaTmdRec D_dryfield_water_hole_801827A4[2] = {
    { 37, 37, 0, 0, { 0, 0 }, D_80139DAC },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaVariant D_dryfield_water_hole_801827BC[13] = {
    { NULL, NULL },
    { D_map_dryfield_8017BB84, D_dryfield_water_hole_801827A4 },
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
    { NULL, NULL },
};

WorldCoordRoomAmbientEntry D_dryfield_water_hole_80182824[9] = {
    { .viewCount = ARRAY_SIZE(D_dryfield_water_hole_80182824) - 1 },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 300, 300, 300, 300 } },
    { .color = { 509, 508, 509, 508 } },
    { .color = { 16, 16, 16, 16 } },
};

WorldCollisionFootstepSounds D_dryfield_water_hole_8018286C = {
    0x10000025,
    0x10000027,
    0x10000029,
};

WorldCollisionFootstepSounds D_dryfield_water_hole_80182878 = {
    0x1000003D,
    0x1000003F,
    0x1000003D,
};

WorldCollisionSurfaceProperties D_dryfield_water_hole_80182884[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_dryfield_water_hole_8018288C[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_dryfield_water_hole_8018286C },
};

WorldCollisionSurfaceProperties D_dryfield_water_hole_80182894[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_dryfield_water_hole_8018289C[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_dryfield_water_hole_801828A4[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_dryfield_water_hole_80182878 },
};

WorldCollisionSurfaceProperties* D_dryfield_water_hole_801828AC[8] = {
    D_dryfield_water_hole_80182884,
    D_dryfield_water_hole_80182884,
    D_dryfield_water_hole_80182894,
    D_dryfield_water_hole_8018289C,
    D_dryfield_water_hole_8018288C,
    D_dryfield_water_hole_80182884,
    D_dryfield_water_hole_801828A4,
    D_dryfield_water_hole_80182884,
};

u8* D_dryfield_water_hole_801828CC = NULL;

s16 D_dryfield_water_hole_801828D0;

static void func_dryfield_water_hole_8017D7DC(Task* arg0);
static void func_dryfield_water_hole_8017D838(Task* task);
static void func_dryfield_water_hole_8017D898(Task* task);

/// Handler for message 0x13F1 in the room's message table: the room takes no
/// action and reports the message as not handled.
s32 func_dryfield_water_hole_8017D5E8(Task* task, s32 msgId, TaskMessageArg arg2, TaskMessageArg arg3)
{
    return 0;
}

/// Handler for message 0x13EE in the room's message table. It copies the
/// incoming record to `out` and, unless `in->queryOnly` is set, answers two
/// queries in `out->room`:
///
/// - 0x19: while the session's stage is 2, 2 once progress nibble 0x3A has
///   reached 2 and 1 before; in any other stage, nibble 0x61 plus one.
/// - 0x26: with nibble 0xC9 set, 2 or 1 by nibble 0x53, plus 2 while nibble
///   0x51 is clear; with 0xC9 clear, 5 or 6 by whether nibble 0x51 is set.
///
/// Always returns 1.
s32 func_dryfield_water_hole_8017D5F0(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    u8 temp;

    *out = *in;
    if (in->areaId == 0x19) {
        temp = gGameSession->location.loc.stage;
        if (temp == 2) {
            if (in->queryOnly == ROOM_EVENT_EXECUTE) {
                if (GameFlag_GetNibble(0x3A) >= 2) {
                    out->room = temp;
                } else {
                    out->room = 1;
                }
            }
        } else if (in->queryOnly == ROOM_EVENT_EXECUTE) {
            out->room = GameFlag_GetNibble(0x61) + 1;
        }
    }
    if (in->areaId == 0x26 && in->queryOnly == ROOM_EVENT_EXECUTE) {
        if (GameFlag_GetNibble(0xC9) != 0) {
            if (GameFlag_GetNibble(0x53) != 0) {
                out->room = 2;
            } else {
                out->room = 1;
            }
            if (GameFlag_GetNibble(0x51) == 0) {
                out->room += 2;
            }
        } else {
            if (GameFlag_GetNibble(0x51) != 0) {
                out->room = 5;
            } else {
                out->room = 6;
            }
        }
    }
    return 1;
}

/// Handler for message 0x13F0 in the room's message table. Only the command 2
/// in `arg2` concerns this room: it arms cap command 2, records it in progress
/// nibble 0x1BD and plays sound event 0x52200004. Always returns 0.
s32 func_dryfield_water_hole_8017D73C(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    if (arg2 == 2) {
        Gp_RunCapCmd1(2);
        GameFlag_SetNibble(0x1BD, 2);
        SndEvt_EnqueueType6(0x52200004, 0, 0);
    }
    return 0;
}

/// Handler for message 0x13EF in the room's message table: the room takes no
/// action and reports the message as not handled.
s32 func_dryfield_water_hole_8017D784(Task* task, s32 msgId, TaskMessageArg arg2, TaskMessageArg arg3)
{
    return 0;
}

/// Handler for message 0x13F2 in the room's message table: plays sound event
/// 0x52200004 for the command 4 in `arg2` and 0x52200005 for 5. Always
/// returns 0.
s32 func_dryfield_water_hole_8017D78C(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    switch (arg2) {
        case 4:
            SndEvt_EnqueueType6(0x52200004, 0, 0);
            break;
        case 5:
            SndEvt_EnqueueType6(0x52200005, 0, 0);
            break;
    }
    return 0;
}

/// Room task entry tick: publishes the room's message table in
/// `Task::msgTable`, claims game pointer slot 7, spawns the room's water task
/// from `D_dryfield_water_hole_8017FC8C` and advances state.
static void func_dryfield_water_hole_8017D7DC(Task* arg0)
{
    arg0->msgTable = D_dryfield_water_hole_8017FC5C;
    Game_SetPtrSlot(arg0, 7);
    Task_SpawnFromTable(D_dryfield_water_hole_8017FC8C, 0, 0, 0);
    arg0->state = (s32)(arg0->state + 1);
}

/// The room task's idle state, entry 1 of its three-state table: does
/// nothing.
static void func_dryfield_water_hole_8017D838(Task* task)
{
}

/// The room task's three states, run from a stack copy by
/// `func_dryfield_water_hole_8017D840`: the entry tick, the idle state, then
/// `taskKill`.
static const TaskFuncTable3 D_dryfield_water_hole_8017D5C4 = {
    { func_dryfield_water_hole_8017D7DC, func_dryfield_water_hole_8017D838, taskKill },
};

/// The room task: copies the three-state table
/// `D_dryfield_water_hole_8017D5C4` onto the stack and runs the entry for the
/// task's current state - the entry tick, the idle state, then `taskKill`.
void func_dryfield_water_hole_8017D840(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_water_hole_8017D5C4;
    sp.funcs[task->state](task);
}

/// Draws each surface in `D_dryfield_water_hole_8017FC98` as two strips of 64
/// semi-transparent Gouraud quads laid side by side along Z, projected through
/// the view matrix. The seam between the strips is lifted by a sine wave that
/// runs along X and scrolls with `D_dryfield_water_hole_801828D0`, which only
/// advances while `gSceneCombatState.actorControl` is clear. The outer edges are coloured
/// (0xFF, 0, 0) and the seam (0x20, 0x20, 0x20); each quad is followed by a
/// draw-mode packet selecting blend mode 2. Quads the projection flags as
/// invalid are skipped. `task` is unused.
static void func_dryfield_water_hole_8017D898(Task* task)
{
    SVECTOR                    v0, v1, v2, v3;
    long                       sxy0, sxy1, sxy2, sxy3;
    long                       p, flag;
    s32                        step;
    s32                        phase;
    _DryfieldWaterHoleSurface* e;
    POLY_G4*                   poly;
    DR_MODE*                   dr;
    s32                        otz;
    s32                        i;
    s32                        half;
    s32                        wave;

    e = D_dryfield_water_hole_8017FC98;
    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionType == 0) {
        D_dryfield_water_hole_801828CC = (u8*)Fs_ActorLoadBase2 + gDisplayState.otBuffer * 0xC000;
    } else {
        D_dryfield_water_hole_801828CC = (u8*)Fs_ActorLoadBase1 + gDisplayState.otBuffer * 0xC000;
    }
    if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING) {
        D_dryfield_water_hole_801828D0++;
    }
    phase                      = -(D_dryfield_water_hole_801828D0 * 16);
    gGfxViewCoord.composeStamp = GRAPHICS_COORD_DIRTY;
    Gp_UpdateCoord(&gGfxViewCoord);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_SetTransMatrix(&gGfxViewCoord.workm);
    for (; e->y != -1; e++) {
        step = e->width / 64;
        half = (s16)e->depth / 2;
        for (i = 0; i < 64; i++) {
            v0.vx = e->x + step * i;
            v0.vy = e->y;
            v0.vz = e->z;
            v1.vx = e->x + step * (i + 1);
            v1.vy = e->y;
            v1.vz = e->z;
            wave  = (rsin(phase + (i << 9)) * 16) >> 12;
            v2.vx = e->x + step * i;
            v2.vy = e->y + wave;
            v2.vz = e->z + half;
            wave  = (rsin(phase + ((i + 1) << 9)) * 16) >> 12;
            v3.vx = e->x + step * (i + 1);
            v3.vy = e->y + wave;
            v3.vz = e->z + half;
            otz   = RotTransPers4(&v0, &v1, &v2, &v3, &sxy0, &sxy1, &sxy2, &sxy3, &p, &flag);
            if (flag >= 0) {
                poly                           = (POLY_G4*)D_dryfield_water_hole_801828CC;
                D_dryfield_water_hole_801828CC = (u8*)(poly + 1);
                setlen(poly, 8);
                setcode(poly, 0x3A);
                GPU_PRIMITIVE_XY_WORD(poly, 0) = sxy0;
                GPU_PRIMITIVE_XY_WORD(poly, 1) = sxy1;
                GPU_PRIMITIVE_XY_WORD(poly, 2) = sxy2;
                GPU_PRIMITIVE_XY_WORD(poly, 3) = sxy3;
                poly->r0                       = 0xFF;
                poly->r1                       = 0xFF;
                poly->g0                       = 0;
                poly->b0                       = 0;
                poly->g1                       = 0;
                poly->b1                       = 0;
                poly->r2                       = 0x20;
                poly->g2                       = 0x20;
                poly->b2                       = 0x20;
                poly->r3                       = 0x20;
                poly->g3                       = 0x20;
                poly->b3                       = 0x20;
                addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                        poly);
                dr                             = (DR_MODE*)D_dryfield_water_hole_801828CC;
                D_dryfield_water_hole_801828CC = (u8*)(dr + 1);
                setlen(dr, 1);
                dr->code[0] = 0xE100004A;
                addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                        dr);
            }
        }
        for (i = 0; i < 64; i++) {
            wave  = (rsin(phase + (i << 9)) * 16) >> 12;
            v0.vx = e->x + step * i;
            v0.vy = e->y + wave;
            v0.vz = e->z + half;
            wave  = (rsin(phase + ((i + 1) << 9)) * 16) >> 12;
            v1.vx = e->x + step * (i + 1);
            v1.vy = e->y + wave;
            v1.vz = e->z + half;
            v2.vx = e->x + step * i;
            v2.vy = e->y;
            v2.vz = e->z + half * 2;
            v3.vx = e->x + step * (i + 1);
            v3.vy = e->y;
            v3.vz = e->z + half * 2;
            otz   = RotTransPers4(&v0, &v1, &v2, &v3, &sxy0, &sxy1, &sxy2, &sxy3, &p, &flag);
            if (flag >= 0) {
                poly                           = (POLY_G4*)D_dryfield_water_hole_801828CC;
                D_dryfield_water_hole_801828CC = (u8*)(poly + 1);
                setlen(poly, 8);
                setcode(poly, 0x3A);
                GPU_PRIMITIVE_XY_WORD(poly, 0) = sxy0;
                GPU_PRIMITIVE_XY_WORD(poly, 1) = sxy1;
                GPU_PRIMITIVE_XY_WORD(poly, 2) = sxy2;
                GPU_PRIMITIVE_XY_WORD(poly, 3) = sxy3;
                poly->r2                       = 0xFF;
                poly->r3                       = 0xFF;
                poly->g2                       = 0;
                poly->b2                       = 0;
                poly->g3                       = 0;
                poly->b3                       = 0;
                poly->r0                       = 0x20;
                poly->g0                       = 0x20;
                poly->b0                       = 0x20;
                poly->r1                       = 0x20;
                poly->g1                       = 0x20;
                poly->b1                       = 0x20;
                addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                        poly);
                dr                             = (DR_MODE*)D_dryfield_water_hole_801828CC;
                D_dryfield_water_hole_801828CC = (u8*)(dr + 1);
                setlen(dr, 1);
                dr->code[0] = 0xE100004A;
                addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                        dr);
            }
        }
    }
}

/// The room's water task: runs its current state -
/// `func_dryfield_water_hole_8017E000` once, then
/// `func_dryfield_water_hole_8017D898`, which draws the surfaces - and each
/// tick sets the session's water height to -0x1A4.
void func_dryfield_water_hole_8017DFA0(Task* task)
{
    TaskFunc states[2] = { func_dryfield_water_hole_8017E000, func_dryfield_water_hole_8017D898 };

    states[task->state](task);
    gGameSession->waterY = -0x1A4;
}

/// The water task's first state: clears the session halfword `field_80`, or
/// `field_7E` while `gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionType` is set, then advances to the drawing state.
static void func_dryfield_water_hole_8017E000(Task* arg0)
{
    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionType == 0) {
        gGameSession->field_80 = 0;
    } else {
        gGameSession->field_7E = 0;
    }
    arg0->state = (s32)(arg0->state + 1);
}

/// Room task. State 0 installs effect ids 0x600FD / 0x600FE in the two shared
/// effect-id slots, records the world positions of parts 14 and 17 of the
/// slot-3 task's model, and advances. State 1, while no event is running and
/// `waterY` is below that model's root, spawns each effect at water level under
/// each part with odds that grow with how far the part moved since last frame,
/// then, once game-flag nibble 0x51 is 1, draws the glowing beams
/// `glowDrawTaperedBeam` renders between the point pairs the
/// current view selects.
void func_dryfield_water_hole_8017E040(Task* arg0)
{
    Task*                     ctl;
    s32                       mask;
    _DryfieldWaterHoleSplash* splash;
    GfxCoord*                 coord;
    GfxCoord*                 ctlCoords;
    GfxCoord*                 part;
    GfxCoord*                 view;
    GfxCoord                  surface;
    s32                       i;
    u32                       rnd;

    ctl       = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    mask      = 1 << gGameSession->location.loc.view;
    splash    = arg0->spawnArg2.pointer;
    coord     = arg0->extra.coordBody->coord;
    ctlCoords = ctl->extra.tmd->coords;
    switch (arg0->state) {
        case 0:
            D_8011574C  = 0x600FD;
            D_80115738  = 0x600FE;
            arg0->state = 1;
            for (i = 0; i < 2; i++) {
                part                                 = &ctl->extra.tmd->coords[14 + i * 3];
                D_dryfield_water_hole_8017FD1C[i].vx = part->workm.t[0];
                D_dryfield_water_hole_8017FD1C[i].vy = part->workm.t[1];
                D_dryfield_water_hole_8017FD1C[i].vz = part->workm.t[2];
            }
            break;
        case 1:
            if (gRoomEffectState->effectControl == ROOM_EFFECT_CONTROL_RUNNING && gGameSession->waterY < ctlCoords->coord.t[1]) {
                view = &gGfxViewCoord;
                for (i = 0; i < 2; i++) {
                    part = &ctl->extra.tmd->coords[14 + i * 3];
                    Gp_UpdateCoord(part);
                    splash->strength = ABS(D_dryfield_water_hole_8017FD1C[i].vx - part->workm.t[0]) +
                                       ABS(D_dryfield_water_hole_8017FD1C[i].vy - part->workm.t[1]) +
                                       ABS(D_dryfield_water_hole_8017FD1C[i].vz - part->workm.t[2]) + 0x20;
                    Gp_WorldToLocal(&gGfxViewCoord.workm, &part->workm, &surface.coord);
                    surface.parent       = view;
                    surface.coord.t[1]   = gGameSession->waterY;
                    surface.composeStamp = GRAPHICS_COORD_DIRTY;
                    Gp_UpdateCoord(&surface);
                    rnd = (gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT);
                    if ((s32)((rnd >> 16) & 0x1FF) < splash->strength) {
                        Gp_SpawnEff(D_8011574C, &surface, 0x40, 0);
                    }
                    splash->strength -= 0x20;
                    rnd               = (gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT);
                    if ((s32)((rnd >> 16) & 0x1FF) < splash->strength) {
                        Gp_SpawnEff(D_80115738, &surface, 0x1202180, 0);
                    }
                    D_dryfield_water_hole_8017FD1C[i].vx = part->workm.t[0];
                    D_dryfield_water_hole_8017FD1C[i].vy = part->workm.t[1];
                    D_dryfield_water_hole_8017FD1C[i].vz = part->workm.t[2];
                }
            }
            if (GameFlag_GetNibble(0x51) == 1) {
                if (mask & 0x18) {
                    glowDrawTaperedBeam(coord, &D_dryfield_water_hole_8017FCC4[0], &D_dryfield_water_hole_8017FCC4[-1], 0x100);
                    glowDrawTaperedBeam(coord, &D_dryfield_water_hole_8017FCC4[2], &D_dryfield_water_hole_8017FCC4[1], 0x100);
                }
                if (mask & 0x50) {
                    glowDrawTaperedBeam(coord, &D_dryfield_water_hole_8017FCDC[0], &D_dryfield_water_hole_8017FCDC[1], 0x100);
                    glowDrawTaperedBeam(coord, &D_dryfield_water_hole_8017FCDC[2], &D_dryfield_water_hole_8017FCDC[3], 0x100);
                }
                if (mask & 0x80) {
                    glowDrawTaperedBeam(coord, &D_dryfield_water_hole_8017FD04[0], &D_dryfield_water_hole_8017FD04[-1], 0x100);
                    glowDrawTaperedBeam(coord, &D_dryfield_water_hole_8017FD04[2], &D_dryfield_water_hole_8017FD04[1], 0x100);
                }
            }
            break;
    }
}

#include "../../shared/glow_draw_tapered_beam.inc.c"

#include "../../shared/water_ripple_task.inc.c"

void func_dryfield_water_hole_8017EC90(Task* task)
{
    waterRippleTask(task);
}

#include "../../shared/water_splash.inc.c"
