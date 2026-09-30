#include "rooms/shelter_b2_elevator_hall.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "gte.h"
#include "types.h"

#include "actors/task_tables.h"

#include "gameplay/display.h"
#include "gameplay/actor_render.h"
#include "gameplay/area.h"
#include "gameplay/areaplace.h"
#include "gameplay/captions.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/direction_input.h"
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

#include "overlay.h"

#include "rooms/room.h"

#include "rooms/room_common.h"

#include "rooms/rooms_shared_8017dcb8.h"
#include "../../shared/room_visual_effects.h"

/// Task descriptor `func_shelter_b2_elevator_hall_8017D610` spawns when a
/// gated event fires.
extern TaskDesc D_shelter_b2_elevator_hall_80183790;

/// Message table `func_shelter_b2_elevator_hall_8017DCBC` installs on its task.
extern GpMsgEntry D_shelter_b2_elevator_hall_801837A8[];

/// Copy of the message that fired a gated event, kept for the task
/// `func_shelter_b2_elevator_hall_8017D774` to warp from.
extern RoomEventMsg D_shelter_b2_elevator_hall_80184D7C;

/// Copy of the request that fired a gated event, whose cap command and voice
/// lines the task `func_shelter_b2_elevator_hall_8017D774` plays.
extern RoomEventReq D_shelter_b2_elevator_hall_80184D88;

/// Event task: plays the recorded request's cap command and voice lines, then
/// copies the recorded message's area, warp and room into the save location,
/// spawns task 0x11 and ends.
void func_shelter_b2_elevator_hall_8017D774(Task* task);

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

void func_shelter_b2_elevator_hall_8017D8E4(Task*);
s32  func_shelter_b2_elevator_hall_8017DAD4(Task*, s32, RoomEventMsg*, RoomEventMsg*);
s32  func_shelter_b2_elevator_hall_8017DC70(Task*, s32, TaskMessageArg, TaskMessageArg);
s32  func_shelter_b2_elevator_hall_8017DC78(Task*, s32, TaskMessageArg, TaskMessageArg);
s32  func_shelter_b2_elevator_hall_8017DC80(Task*, s32, TaskMessageArg, TaskMessageArg);
s32  func_shelter_b2_elevator_hall_8017DC88(Task*, s32, s32, TaskMessageArg);

TaskDesc D_shelter_b2_elevator_hall_80183790 = { 0, 32, func_shelter_b2_elevator_hall_8017D774, { .model = NULL } };

TaskDesc D_shelter_b2_elevator_hall_8018379C = { 0, 32, func_shelter_b2_elevator_hall_8017D8E4, { .model = NULL } };

GpMsgEntry D_shelter_b2_elevator_hall_801837A8[6] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, func_shelter_b2_elevator_hall_8017DAD4 },
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

#define ROOM_FX_HALO_STORAGE_INITIALIZER { \
    { 0, 1, 2 },                           \
    { 2, 1, 0 },                           \
    { 0, 2, 1 },                           \
}
#define ROOM_FX_HALO_STORAGE_TYPE  RoomHaloShade
#define ROOM_FX_HALO_STORAGE_BOUND [3]
#include "../../shared/room_visual_effects_halo_data.inc.c"

static inline RoomHaloShade* RoomFx_GetHaloShades(void)
{
    return RoomFx_HaloShades;
}
#undef ROOM_FX_HALO_STORAGE_INITIALIZER
#undef ROOM_FX_HALO_STORAGE_TYPE
#undef ROOM_FX_HALO_STORAGE_BOUND

#include "../../shared/room_visual_effects_trail_data.inc.c"

u8* D_shelter_b2_elevator_hall_801838DC[1] = {
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
#include "assets/shelter_b2_elevator_hall_collision_067F4_normals.inc"
};

SVECTOR D_shelter_b2_elevator_hall_801839C4[48] = {
#include "assets/shelter_b2_elevator_hall_collision_067F4_verts.inc"
};

GpGridFace D_shelter_b2_elevator_hall_80183B44[28] = {
#include "assets/shelter_b2_elevator_hall_collision_067F4_faces.inc"
};

s16 D_shelter_b2_elevator_hall_80183C94[124] = {
#include "assets/shelter_b2_elevator_hall_collision_067F4_cells.inc"
};

#define GRID_CELL(i) (&D_shelter_b2_elevator_hall_80183C94[i])
s16* D_shelter_b2_elevator_hall_80183D8C[10] = {
#include "assets/shelter_b2_elevator_hall_collision_067F4_table.inc"
};
#undef GRID_CELL

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

SpriteBatch D_shelter_b2_elevator_hall_80183ED4[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b2_elevator_hall_80183EE4[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b2_elevator_hall_80183EF4[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
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

SpriteBatch D_shelter_b2_elevator_hall_80183FCC[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 10, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
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

SpriteBatch D_shelter_b2_elevator_hall_801840E8[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 12, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b2_elevator_hall_80184100[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b2_elevator_hall_80184110[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
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
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -681, -659, -1106 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2457, 2457, 2457 }, { 0, 0 } }, 900, 1900 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -681, -659, 1137 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2457, 2457, 2457 }, { 0, 0 } }, 900, 1900 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 1648, -659, 1137 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2457, 2457, 2457 }, { 0, 0 } }, 900, 1900 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 1047, -659, -1192 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2457, 2457, 2457 }, { 0, 0 } }, 900, 1900 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 6077, -659, -1034 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2457, 2457, 2457 }, { 0, 0 } }, 900, 1900 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 8608, -659, -855 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2457, 2457, 2457 }, { 0, 0 } }, 900, 1900 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 8600, -659, 2166 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2457, 2457, 2457 }, { 0, 0 } }, 900, 1900 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 4834, -659, 2957 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2457, 2457, 2457 }, { 0, 0 } }, 900, 1900 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 7805, -6652, 1436 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3279, 3368, 3448 }, { 0, 0 } }, 2000, 8000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 2330, -6652, -263 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3267, 3308, 3588 }, { 0, 0 } }, 2000, 8000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -5813, -5652, -97 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3366, 3466, 3708 }, { 0, 0 } }, 2000, 0x2801 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -8513, -659, 35 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2457, 2457, 2457 }, { 0, 0 } }, 900, 1900 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -7513, -659, -1106 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2457, 2457, 2457 }, { 0, 0 } }, 900, 1900 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -4617, -659, -1134 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2457, 2457, 2457 }, { 0, 0 } }, 900, 1900 },
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

AreaPlacement D_shelter_b2_elevator_hall_80184ADC[8] = {
    { 26, 0, 0, 7500, 0, 4150, 1800, 0, 0, 2, 0 },
    { 26, 0, 0, 7000, 0, 1950, 1300, 0, 0, 2, 0 },
    { 26, 0, 0, 5300, 0, 3600, 1700, 0, 0, 2, 0 },
    { 26, 0, 0, 5400, 0, 1700, 1500, 0, 0, 2, 0 },
    { 26, 0, 0, 1850, 0, 350, 1250, 0, 0, 2, 0 },
    { 26, 0, 0, 300, 0, 600, 1200, 0, 0, 2, 0 },
    { 26, 0, 0, 300, 0, -800, 900, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaPlacement D_shelter_b2_elevator_hall_80184B5C[3] = {
    { 3, 0, 0, 6500, 0, 3300, 2048, 0, 0, 2, 0 },
    { 3, 0, 1, -4000, 0, 0, 1024, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaPlacement D_shelter_b2_elevator_hall_80184B8C[4] = {
    { 49, 2, 0, -7800, 0, -200, 1200, 0, 0, 2, 0 },
    { 49, 2, 0, 250, 0, 600, 1600, 0, 0, 2, 0 },
    { 49, 2, 0, 4350, 0, 500, 1800, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaPlacement D_shelter_b2_elevator_hall_80184BCC[6] = {
    { 21, 3, 0, 7500, -1800, -1400, 0, 0, 0, 2, 0 },
    { 21, 3, 0, 5500, -1800, -1400, 0, 0, 0, 2, 0 },
    { 21, 3, 0, 7500, -1800, 4900, 2048, 0, 0, 2, 0 },
    { 21, 3, 0, 5500, -1800, 4900, 2048, 0, 0, 2, 0 },
    { 57, 3, 1, 6500, 0, 3000, 2048, 0, 2, 4, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaPlacement D_shelter_b2_elevator_hall_80184C2C[5] = {
    { 21, 3, 0, 8500, -1800, 4900, 2048, 0, 0, 2, 7 },
    { 21, 3, 0, 6500, -1800, 4900, 2048, 0, 0, 2, 7 },
    { 57, 9, 1, -2000, 0, 0, 1024, 0, 2, 4, 0 },
    { 57, 0, 0, 9600, 0, -500, 3072, 0, 2, 4, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
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

GpRoomParamRec* D_shelter_b2_elevator_hall_80184D5C[8] = {
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

RoomEventReq D_shelter_b2_elevator_hall_80184D88;

static s32 func_shelter_b2_elevator_hall_8017D610(RoomEventReq* req, RoomEventMsg* msg);

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

    flag                                   = req->flagId;
    D_shelter_b2_elevator_hall_80184D84[0] = 0;
    neg                                    = flag < 0;
    got                                    = (s16)flag;
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
        if (msg->queryOnly == ROOM_EVENT_EXECUTE) {
            Gp_RunCapCmd1(req->field_4);
            Gp_SetNibbleIf(msg->flagId, 2);
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
            gDisplayState.spriteVariant       = 1;
            Mc_SaveData[0].state.at4.loc.area = D_shelter_b2_elevator_hall_80184D7C.areaId;
            Mc_SaveData[0].state.at4.loc.warp = D_shelter_b2_elevator_hall_80184D7C.warp;
            Mc_SaveData[0].state.at4.loc.room = (u8)D_shelter_b2_elevator_hall_80184D7C.room;
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
            msg.room      = 1;
            msg.queryOnly = ROOM_EVENT_EXECUTE;
            msg.areaId    = Mc_SaveData[0].state.at4.loc.area;
            msg.warp      = Mc_SaveData[0].state.at4.loc.warp;
            msg2          = msg;
            func_map_shelter_80179A04(&msg, &msg2);
            gDisplayState.spriteVariant       = 1;
            Mc_SaveData[0].state.at4.loc.warp = msg2.warp;
            Mc_SaveData[0].state.at4.loc.room = msg2.room;
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
/// answers 0 and, unless `in->queryOnly` asks for a dry run, either sets the
/// message's nibble and runs CAP command 4 while nibble 0xBA is clear, or runs
/// CAP command 5 and spawns the room's task once it is set. Anything else
/// answers 1.
s32 func_shelter_b2_elevator_hall_8017DAD4(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    RoomEventReq req;
    s32          ret;

    *out = *in;
    func_map_shelter_80179A04(in, out);
    if (in->areaId == 0x21) {
        req.field_0 = 1;
        req.field_4 = 1;
        req.field_8 = 0x541B0007;
        req.field_C = 0x541B0005;
        req.flagId  = 0xAB;
        req.itemId  = 0;
        return func_shelter_b2_elevator_hall_8017D610(&req, out);
    }
    if (in->areaId == 0x1C) {
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
    if (in->areaId == 0x1A) {
        if (GameFlag_GetNibble(0xBA) == 0) {
            if (in->queryOnly == ROOM_EVENT_EXECUTE) {
                Gp_SetNibbleIf(in->flagId, 2);
                Gp_RunCapCmd1(4);
            }
            return 0;
        }
        if (in->queryOnly == ROOM_EVENT_EXECUTE) {
            Gp_RunCapCmd(5, 0);
            Task_SpawnFromTable(&D_shelter_b2_elevator_hall_8018379C, 0, 0x541B0001, 0);
        }
        return 0;
    }
    return 1;
}

s32 func_shelter_b2_elevator_hall_8017DC70(Task* task, s32 msgId, TaskMessageArg arg2, TaskMessageArg arg3)
{
    return 0;
}

s32 func_shelter_b2_elevator_hall_8017DC78(Task* task, s32 msgId, TaskMessageArg arg2, TaskMessageArg arg3)
{
    return 0;
}

s32 func_shelter_b2_elevator_hall_8017DC80(Task* task, s32 msgId, TaskMessageArg arg2, TaskMessageArg arg3)
{
    return 0;
}

s32 func_shelter_b2_elevator_hall_8017DC88(Task* arg0, s32 arg1, s32 arg2, TaskMessageArg arg3)
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

        scratch = SCRATCH_STACK_CURSOR_SLOT;
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
            prim           = gGpuPrimCursor;
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
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);

            prim           = gGpuPrimCursor;
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
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        } while (ang < 0x1000);

        r   = (u8)rh;
        g   = (u8)gh;
        b   = (u8)bh;
        ang = 0x200;
        do {
            prim           = gGpuPrimCursor;
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
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);

            prim           = gGpuPrimCursor;
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
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        } while (ang < 0x1000);
    }
    SCRATCH_POP_BYTES(0x14);
}

#include "../../shared/room_visual_effects.inc.c"

void func_shelter_b2_elevator_hall_8017F1D8(Task* task)
{
    RoomFx_MoteTask(task);
}

#include "../../shared/room_visual_effects_halo.inc.c"

void func_shelter_b2_elevator_hall_8017FF20(Task* arg0)
{
    RoomFx_HaloTask(arg0);
}

void func_shelter_b2_elevator_hall_801802B8(Task* arg0)
{
    RoomFx_OrangeBurstTask(arg0);
}

#include "../../shared/room_visual_effects_glow_quad.inc.c"
#include "../../shared/room_visual_effects_flash.inc.c"

void func_shelter_b2_elevator_hall_801816C8(Task* arg0)
{
    RoomFx_SparkEmitterTask(arg0);
}

#include "../../shared/room_visual_effects_flash_task.inc.c"

void func_shelter_b2_elevator_hall_801817FC(Task* arg0)
{
    RoomFx_FlashTask(arg0);
}

#include "../../shared/room_visual_effects_trails.inc.c"

void func_shelter_b2_elevator_hall_80182260(Task* task)
{
#include "../../shared/room_visual_effects_trail_task.inc.c"
}

#include "../../shared/room_visual_effects_sparks.inc.c"

void func_shelter_b2_elevator_hall_80182B48(Task* task)
{
    RoomFx_SparkBurstTask(task);
}

#include "../../shared/room_visual_effects_glow.inc.c"
