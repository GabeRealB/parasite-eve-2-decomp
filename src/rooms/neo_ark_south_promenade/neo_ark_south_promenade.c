#include "rooms/neo_ark_south_promenade.h"

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
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/direction_input.h"
#include "gameplay/effects.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/light.h"
#include "gameplay/message.h"
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
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "mapui/map_neo_ark.h"

#include "rooms/room_common.h"

/// The room's message table, which the message-driven task installs.
extern GpMsgEntry D_neo_ark_south_promenade_8017F6B4[];

/// The smoke trail's two spawn offsets: `[0]` places the object's own frame
/// and `[1]` the second trail's frame. `D_neo_ark_south_promenade_8017F6DC[1]` is
/// `[1]` under its own name, which the per-frame path reads directly.

static void func_neo_ark_south_promenade_8017D62C(Task* task);
static void func_neo_ark_south_promenade_8017D670(Task* task);
static void func_neo_ark_south_promenade_8017D9C4(GfxCoord* arg0, s32 arg1, s32 arg2, u8* rgb);
static void func_neo_ark_south_promenade_8017DDF0(GfxCoord* arg0, s16 arg1, u8* rgb);
static void func_neo_ark_south_promenade_8017E674(GfxCoord* arg0, GfxCoord* arg1, s16 arg2, s16 arg3);
static void func_neo_ark_south_promenade_8017ECF4(GfxCoord* arg0, s16 arg1, u8* arg2);

/// State table of the room's message-driven task, indexed by `Task::state`:
/// install the message table, idle, then kill the task.
static const TaskFuncTable3 D_neo_ark_south_promenade_8017D5C4 = {
    { func_neo_ark_south_promenade_8017D62C, func_neo_ark_south_promenade_8017D670, taskKill },
};

s32 func_neo_ark_south_promenade_8017D5D0(Task*, s32, GpMessageArg, GpMessageArg);
s32 func_neo_ark_south_promenade_8017D5D8(Task*, s32, GpSaveLoc*, GpSaveLoc*);
s32 func_neo_ark_south_promenade_8017D61C(Task*, s32, GpMessageArg, GpMessageArg);
s32 func_neo_ark_south_promenade_8017D624(Task*, s32, GpMessageArg, GpMessageArg);

extern GpGridParams   D_neo_ark_south_promenade_8017FD8C[1];
extern GpObj3A        D_neo_ark_south_promenade_8018094C[1];
extern GpObj4C        D_neo_ark_south_promenade_801804E8[6];
extern GpObj4C        D_neo_ark_south_promenade_801806B0[6];
extern GpRoomCoordSet D_neo_ark_south_promenade_801804D0[1];

GpMsgEntry D_neo_ark_south_promenade_8017F6B4[5] = {
    { 5102, func_neo_ark_south_promenade_8017D5D8 },
    { 5105, func_neo_ark_south_promenade_8017D5D0 },
    { 5103, func_neo_ark_south_promenade_8017D624 },
    { 5104, func_neo_ark_south_promenade_8017D61C },
    { 0x7FFFFFFF, NULL },
};

SVECTOR D_neo_ark_south_promenade_8017F6DC[2] = {
    { 0, 190, -15, 0 },
    { 0, 1085, 180, 0 },
};

GpRoomCoordRec D_neo_ark_south_promenade_8017F6EC[1] = {
    { D_neo_ark_south_promenade_801804D0, NULL },
};

GpRoomObjRec D_neo_ark_south_promenade_8017F6F4[1] = {
    { D_neo_ark_south_promenade_8017FD8C, D_neo_ark_south_promenade_801804E8, D_neo_ark_south_promenade_801806B0, D_neo_ark_south_promenade_8018094C },
};

u8* D_neo_ark_south_promenade_8017F704[1] = {
    D_8010CAF8,
};

GpViewCountRec D_neo_ark_south_promenade_8017F708[1] = {
    { { .bytes = { 5, 0 } } },
};

GpWarpRec D_neo_ark_south_promenade_8017F70C[2] = {
    { { .words = { 1024, 1137, 0, 2500 } }, { 0, 0, 0, 0 }, { .words = { 1024, 1137, 0, 2500 } }, { 0, 0, 0, 0 }, 0x55130002, 0x55130001, 0, 5, 0, 0 },
    { { .words = { 3072, 0x2A30, 0, 0x3200 } }, { 0, 0, 0, 0 }, { .words = { 3072, 0x2A30, 0, 0x3200 } }, { 0, 0, 0, 0 }, 0, 0, 0, 2, 0, 0 },
};

SVECTOR D_neo_ark_south_promenade_8017F77C[13] = {
#include "assets/neo_ark_south_promenade_collision_027CC_normals.inc"
};

SVECTOR D_neo_ark_south_promenade_8017F7E4[81] = {
#include "assets/neo_ark_south_promenade_collision_027CC_verts.inc"
};

GpGridFace D_neo_ark_south_promenade_8017FA6C[35] = {
#include "assets/neo_ark_south_promenade_collision_027CC_faces.inc"
};

s16 D_neo_ark_south_promenade_8017FC10[158] = {
#include "assets/neo_ark_south_promenade_collision_027CC_cells.inc"
};

#define GRID_CELL(i) (&D_neo_ark_south_promenade_8017FC10[i])
s16* D_neo_ark_south_promenade_8017FD4C[16] = {
#include "assets/neo_ark_south_promenade_collision_027CC_table.inc"
};
#undef GRID_CELL

GpGridParams D_neo_ark_south_promenade_8017FD8C[1] = {
    { NULL, D_neo_ark_south_promenade_8017F77C, D_neo_ark_south_promenade_8017F7E4, D_neo_ark_south_promenade_8017FA6C, D_neo_ark_south_promenade_8017FD4C, -700, -500, 4, 4, 4000, 35 },
};

GpViewRec D_neo_ark_south_promenade_8017FDB0[5] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { -7000, 0x58D6, -7000 } }, 289 },
    { { { { 4002, 0, 871 }, { 35, 4092, -163 }, { -871, 167, 3998 } }, { -0x3070, 1502, -3900 } }, 257 },
    { { { { -3965, 0, 1025 }, { 54, 4090, 210 }, { -1023, 217, -3960 } }, { -0x31E2, 1650, -0x2BC0 } }, 257 },
    { { { { -954, 0, 3983 }, { 197, 4090, 47 }, { -3978, 203, -953 } }, { -0x3070, 1690, -3200 } }, 207 },
    { { { { -870, 0, 4002 }, { 776, 4018, 168 }, { -3926, 794, -853 } }, { -8000, 2042, -3300 } }, 207 },
};

GpSprtCmd D_neo_ark_south_promenade_8017FE64[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_neo_ark_south_promenade_8017FE74[25] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -144, 24, 912, { .fields = { 120, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -160, -120, 912, { .fields = { 64, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -160, 56, 900, { .fields = { 8, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -160, 16, 900, { .fields = { 8, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 56, -48, 1856, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 112, 8, 1837, { .fields = { 32, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 112, -40, 1837, { .fields = { 32, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 112, -88, 1837, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, 128, -112, 1837, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 64, -88, 1837, { .fields = { 80, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 64, -40, 1837, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 64, 8, 1837, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 16, 8, 1856, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 16, -40, 1856, { .fields = { 48, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, -16, -16, 1856, { .fields = { 104, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, -16, 8, 1856, { .fields = { 96, 192 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 8 } }, 136, -64, 1905, { .fields = { 120, 240 } }, 128, 128, 128, 2 },
    { 143, 0x4000, { .fields = { 8, 8 } }, 112, -48, 1905, { .fields = { 112, 248 } }, 128, 128, 128, 2 },
    { 143, 0x4000, { .fields = { 16, 40 } }, 0, -8, 1905, { .fields = { 24, 144 } }, 128, 128, 128, 2 },
    { 143, 0x4000, { .fields = { 24, 40 } }, 16, -24, 1905, { .fields = { 40, 144 } }, 128, 128, 128, 2 },
    { 143, 0x4000, { .fields = { 24, 40 } }, 40, -40, 1905, { .fields = { 8, 40 } }, 128, 128, 128, 2 },
    { 143, 0x4000, { .fields = { 24, 40 } }, 64, -56, 1905, { .fields = { 8, 80 } }, 128, 128, 128, 2 },
    { 143, 0x4000, { .fields = { 24, 40 } }, 88, -72, 1905, { .fields = { 8, 0 } }, 128, 128, 128, 2 },
    { 143, 0x4000, { .fields = { 24, 40 } }, 112, -88, 1905, { .fields = { 24, 184 } }, 128, 128, 128, 2 },
    { 143, 0x4000, { .fields = { 24, 40 } }, 136, -104, 1905, { .fields = { 0, 120 } }, 128, 128, 128, 2 },
};

GpSprtCmd D_neo_ark_south_promenade_80180068[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 4, 0, 0, { 1, 0 } },
    { 4, 12, 0, 0, { 2, 0 } },
    { 16, 9, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_neo_ark_south_promenade_80180090[39] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 24, -120, 1862, { .fields = { 24, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 80, -120, 1625, { .fields = { 16, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, 112, 24, 1375, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 48 } }, 80, 24, 1625, { .fields = { 104, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, 56, 24, 1837, { .fields = { 56, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 56, 8, 1837, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, 112, -24, 1375, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 48 } }, 80, -24, 1625, { .fields = { 104, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, 112, -72, 1375, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, 112, -120, 1375, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, 80, -72, 1625, { .fields = { 0, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 80, -88, 1625, { .fields = { 72, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, 56, -72, 1837, { .fields = { 64, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, 48, -120, 1837, { .fields = { 8, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 24, -120, 2306, { .fields = { 8, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 112, 8, 1862, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, 72, 8, 1862, { .fields = { 72, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 56, 8, 1862, { .fields = { 88, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 88, -40, 1862, { .fields = { 104, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 112, -40, 1862, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 112, -88, 1862, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 32 } }, 112, -120, 1862, { .fields = { 40, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, 72, -88, 1862, { .fields = { 88, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 32 } }, 72, -120, 1862, { .fields = { 56, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, 56, -120, 1862, { .fields = { 88, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 32 } }, 64, -88, 1862, { .fields = { 96, 192 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 8 } }, 24, -120, 2175, { .fields = { 32, 240 } }, 128, 128, 128, 2 },
    { 143, 0x4000, { .fields = { 8, 8 } }, 56, 8, 1862, { .fields = { 56, 240 } }, 128, 128, 128, 2 },
    { 143, 0x4000, { .fields = { 48, 48 } }, 112, -16, 1625, { .fields = { 56, 192 } }, 128, 128, 128, 2 },
    { 143, 0x4000, { .fields = { 48, 48 } }, 112, -64, 1625, { .fields = { 32, 144 } }, 128, 128, 128, 2 },
    { 143, 0x4000, { .fields = { 48, 48 } }, 112, -112, 1625, { .fields = { 8, 192 } }, 128, 128, 128, 2 },
    { 142, 0x4000, { .fields = { 48, 8 } }, 112, -120, 1625, { .fields = { 56, 88 } }, 128, 128, 128, 2 },
    { 143, 0x4000, { .fields = { 48, 48 } }, 64, -16, 1812, { .fields = { 32, 96 } }, 128, 128, 128, 2 },
    { 143, 0x4000, { .fields = { 48, 48 } }, 64, -64, 1812, { .fields = { 40, 48 } }, 128, 128, 128, 2 },
    { 143, 0x4000, { .fields = { 8, 16 } }, 56, -64, 1862, { .fields = { 64, 240 } }, 128, 128, 128, 2 },
    { 143, 0x4000, { .fields = { 48, 48 } }, 64, -112, 1812, { .fields = { 32, 0 } }, 128, 128, 128, 2 },
    { 143, 0x4000, { .fields = { 48, 8 } }, 64, -120, 1750, { .fields = { 16, 248 } }, 128, 128, 128, 2 },
    { 142, 0x4000, { .fields = { 16, 24 } }, 48, -112, 1862, { .fields = { 64, 136 } }, 128, 128, 128, 2 },
    { 143, 0x4000, { .fields = { 16, 8 } }, 48, -120, 1862, { .fields = { 40, 240 } }, 128, 128, 128, 2 },
};

GpSprtCmd D_neo_ark_south_promenade_8018039C[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 14, 0, 0, { 1, 0 } },
    { 14, 12, 0, 0, { 2, 0 } },
    { 26, 13, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_neo_ark_south_promenade_801803C4[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_neo_ark_south_promenade_801803D4[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtRec D_neo_ark_south_promenade_801803E4[5] = {
    { { .empty = D_neo_ark_south_promenade_8017FE64 }, D_neo_ark_south_promenade_8017FE64, NULL },
    { { .elements = D_neo_ark_south_promenade_8017FE74 }, D_neo_ark_south_promenade_80180068, NULL },
    { { .elements = D_neo_ark_south_promenade_80180090 }, D_neo_ark_south_promenade_8018039C, NULL },
    { { .empty = D_neo_ark_south_promenade_801803C4 }, D_neo_ark_south_promenade_801803C4, NULL },
    { { .empty = D_neo_ark_south_promenade_801803D4 }, D_neo_ark_south_promenade_801803D4, NULL },
};

GpLight D_neo_ark_south_promenade_80180420[2] = {
    { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 392, -740, 260 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 4997, 4915, 4894, { 0, 0 } },
    { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -332, 260, -220 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 1310, 1290, 1228, { 0, 0 } },
};

GpRoomCoordSet D_neo_ark_south_promenade_801804D0[1] = {
    { 2, D_neo_ark_south_promenade_80180420, 0, NULL, 0, NULL },
};

GpObj4C D_neo_ark_south_promenade_801804E8[6] = {
    { NULL, NULL, NULL, { 5005, -1568, 2682, 0 }, { { 44, -1904, -2937, 0 }, { -44, -1904, 2938, 0 }, { 44, 1904, -2937, 0 }, { -44, 1904, 2938, 0 } }, { 4109, 0, 60, 0 }, { 0, 0, 4096, 0 }, 3500, 0, 4, 5, 1, 0 },
    { NULL, NULL, NULL, { 5272, -1600, 2635, 0 }, { { -17, -1904, 3156, 0 }, { 17, -1904, -3155, 0 }, { -17, 1904, 3156, 0 }, { 17, 1904, -3155, 0 } }, { -4101, 0, -23, 0 }, { 0, 0, 4096, 0 }, 3683, 0, 5, 4, 1, 0 },
    { NULL, NULL, NULL, { 9087, -1441, 2401, 0 }, { { 309, -1904, 2737, 0 }, { -308, -1904, -2736, 0 }, { 309, 1904, 2737, 0 }, { -308, 1904, -2736, 0 } }, { -4071, 0, 458, 0 }, { 0, 0, 4096, 0 }, 3347, 0, 4, 3, 1, 0 },
    { NULL, NULL, NULL, { 8784, -1601, 2400, 0 }, { { -323, -1904, -2719, 0 }, { 322, -1904, 2718, 0 }, { -323, 1904, -2719, 0 }, { 322, 1904, 2718, 0 } }, { 4084, 0, -485, 0 }, { 0, 0, 4096, 0 }, 3328, 0, 3, 4, 1, 0 },
    { NULL, NULL, NULL, { 0x2C11, -1440, 7472, 0 }, { { 3787, -1904, 557, 0 }, { -3788, -1904, -558, 0 }, { 3787, 1904, 557, 0 }, { -3788, 1904, -558, 0 } }, { -597, 0, 4053, 0 }, { 0, 0, 4096, 0 }, 4252, 0, 2, 3, 1, 0 },
    { NULL, NULL, NULL, { 0x2BD1, -1504, 7632, 0 }, { { -3440, -1904, -505, 0 }, { 3439, -1904, 504, 0 }, { -3440, 1904, -505, 0 }, { 3439, 1904, 504, 0 } }, { 594, 0, -4057, 0 }, { 0, 0, 4096, 0 }, 3957, 0, 3, 2, 129, 0 },
};

GpObj4C D_neo_ark_south_promenade_801806B0[6] = {
    { NULL, NULL, NULL, { 1104, -48, 2416, 0 }, { { -496, 0, -1104, 0 }, { 496, 0, -1104, 0 }, { -496, 0, 1104, 0 }, { 496, 0, 1104, 0 } }, { 0, 4113, 0, 0 }, { 4096, 0, 0, 0 }, 1207, 0, 18, 18, 2, 0 },
    { NULL, NULL, NULL, { 0x2D20, -1248, 0x3070, 0 }, { { 0, 1280, -1072, 0 }, { 0, -1280, -1072, 0 }, { 0, 1280, 1072, 0 }, { 0, -1280, 1072, 0 } }, { 4106, 0, 0, 0 }, { -4096, 0, 0, 0 }, 1668, 0, 7, 35, 3, 0 },
    { NULL, NULL, NULL, { 0x2A30, -64, 0x30A0, 0 }, { { -288, 0, -592, 0 }, { 288, 0, -592, 0 }, { -288, 0, 592, 0 }, { 288, 0, 592, 0 } }, { 0, 4107, 0, 0 }, { -4096, 0, 0, 0 }, 655, 1, 24, 64, 2, 0 },
    { NULL, NULL, NULL, { 6464, -64, 912, 0 }, { { -5648, 0, -480, 0 }, { 5648, 0, -480, 0 }, { -5648, 0, 480, 0 }, { 5648, 0, 480, 0 } }, { 0, 4112, 0, 0 }, { 0, 0, 4096, 0 }, 5655, 2, 2, 0, 2, 0 },
    { NULL, NULL, NULL, { 4720, -64, 3936, 0 }, { { -3872, 0, -480, 0 }, { 3872, 0, -480, 0 }, { -3872, 0, 480, 0 }, { 3872, 0, 480, 0 } }, { 0, 4096, 0, 0 }, { 0, 0, -4096, 0 }, 3899, 2, 1, 0, 2, 0 },
    { NULL, NULL, NULL, { 0x2B2F, -64, 1375, 0 }, { { -1874, 0, -1634, 0 }, { 2368, 0, 812, 0 }, { -2367, 0, -811, 0 }, { 1875, 0, 1635, 0 } }, { 0, 4099, 0, 0 }, { -2276, 0, 3406, 0 }, 2495, 2, 2, 0, 130, 0 },
};

GpAreaTmdRec D_neo_ark_south_promenade_80180878[1] = {
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_neo_ark_south_promenade_80180884[1] = {
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_neo_ark_south_promenade_80180890[2] = {
    { 38, 38, 0, 0, { 0, 0 }, D_80137D74 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_neo_ark_south_promenade_801808A8[2] = {
    { 56, 56, 0, 0, { 0, 0 }, D_801482C0 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_neo_ark_south_promenade_801808C0[3] = {
    { 20, 20, 0, 0, { 0, 0 }, D_80147DF0 },
    { 23, 23, 1, 0, { 0, 0 }, D_8015FAB8 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaVariant D_neo_ark_south_promenade_801808E4[13] = {
    { NULL, NULL },
    { NULL, NULL },
    { D_map_neo_ark_8017BDB0, D_neo_ark_south_promenade_80180878 },
    { D_map_neo_ark_8017BDC0, D_neo_ark_south_promenade_80180884 },
    { D_map_neo_ark_8017BDD0, D_neo_ark_south_promenade_80180890 },
    { D_map_neo_ark_8017BE40, D_neo_ark_south_promenade_801808A8 },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { D_map_neo_ark_8017BE70, D_neo_ark_south_promenade_801808C0 },
    { NULL, NULL },
};

GpObj3A D_neo_ark_south_promenade_8018094C[1] = {
    { NULL, NULL, { 4912, -1568, 7344, 0 }, { { -4560, 2496, 2832, 0 }, { 4560, 2496, -2832, 0 }, { -4560, -2496, 2832, 0 }, { 4560, -2496, -2832, 0 } }, { 2162, 0, 3481, 0 }, { 16, 23 }, 129, 0 },
};

s32 D_neo_ark_south_promenade_80180988[3] = {
    0x10000041,
    0x10000043,
    0x10000041,
};

GpRoomParamRec D_neo_ark_south_promenade_80180994[1] = {
    { 0, 0, 1, 0, NULL },
};

GpRoomParamRec D_neo_ark_south_promenade_8018099C[1] = {
    { 0, 0, 1, 0, D_neo_ark_south_promenade_80180988 },
};

GpRoomParamRec D_neo_ark_south_promenade_801809A4[1] = {
    { 0, 1, 0, 0, D_neo_ark_south_promenade_80180988 },
};

GpRoomParamRec* D_neo_ark_south_promenade_801809AC[8] = {
    D_neo_ark_south_promenade_80180994,
    D_neo_ark_south_promenade_8018099C,
    D_neo_ark_south_promenade_801809A4,
    D_neo_ark_south_promenade_80180994,
    D_neo_ark_south_promenade_80180994,
    D_neo_ark_south_promenade_80180994,
    D_neo_ark_south_promenade_80180994,
    D_neo_ark_south_promenade_80180994,
};

/// Message handler the room's message table names for one of its entries:
/// accepts the message and does nothing.
s32 func_neo_ark_south_promenade_8017D5D0(Task* task, s32 msgId, GpMessageArg arg2, GpMessageArg arg3)
{
    return 0;
}

/// Message handler the room's message table names for one of its entries:
/// copies the incoming message onto the outgoing one, passes both to
/// `func_map_neo_ark_80179B14` and returns 1.
s32 func_neo_ark_south_promenade_8017D5D8(Task* arg0, s32 arg1, GpSaveLoc* in, GpSaveLoc* out)
{
    *out = *in;
    func_map_neo_ark_80179B14(in, out);
    return 1;
}

/// Message handler the room's message table names for one of its entries:
/// accepts the message and does nothing.
s32 func_neo_ark_south_promenade_8017D61C(Task* task, s32 msgId, GpMessageArg arg2, GpMessageArg arg3)
{
    return 0;
}

/// Message handler the room's message table names for one of its entries:
/// accepts the message and does nothing.
s32 func_neo_ark_south_promenade_8017D624(Task* task, s32 msgId, GpMessageArg arg2, GpMessageArg arg3)
{
    return 0;
}

/// State 0 of the room's message-driven task: parks the room's message table
/// in `Task::msgTable`, publishes the task in pointer slot 7 and advances to
/// state 1.
static void func_neo_ark_south_promenade_8017D62C(Task* task)
{
    task->msgTable = D_neo_ark_south_promenade_8017F6B4;
    Game_SetPtrSlot(task, 7);
    task->state = (s32)(task->state + 1);
}

/// State 1 of the room's message-driven task: idles until the task is killed.
static void func_neo_ark_south_promenade_8017D670(Task* task)
{
}

/// Dispatches the room's message-driven task through its three-state table,
/// copied onto the stack before the call.
void func_neo_ark_south_promenade_8017D678(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_neo_ark_south_promenade_8017D5C4;
    sp.funcs[task->state](task);
}

void func_neo_ark_south_promenade_8017D6D0(Task* arg0)
{
    if (arg0->state == 0) {
        D_80115758  = 0x601DE;
        D_8011572C  = 0x601FA;
        D_80115750  = 0x60216;
        arg0->state = 1;
    }
}

/// Expanding flash burst. State 0 derives the step from the spawn argument;
/// state 1 draws a disc at the current radius, a half-bright one at twice it
/// and a ring closing in from 0x300 while the level ramps up, finishing with a
/// full-screen fade quad; state 2 fades an afterglow at three times the radius
/// back out before releasing the work block. It releases the block early once
/// the room's event state reaches 4.
void func_neo_ark_south_promenade_8017D720(Task* task)
{
    GpEffWork* work;
    GfxCoord*  coord;
    u8         rgb[3];

    work  = task->spawnArg2.pointer;
    coord = task->extra.disp2d->coord;
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
                func_neo_ark_south_promenade_8017DDF0(coord, work->angle, rgb);
                rgb[0] >>= 1;
                rgb[1] >>= 1;
                rgb[2] >>= 1;
                func_neo_ark_south_promenade_8017DDF0(coord, (s16)((u16)work->angle * 2), rgb);
                func_neo_ark_south_promenade_8017D9C4(coord, (s16)(0x300 - (u16)work->angle * 2), 0x80, rgb);
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
                    func_neo_ark_south_promenade_8017ECF4(coord, (s16)(work->angle * 3), rgb);
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

/// Projects the coordinate's world position through `GsWSMATRIX` and, when
/// the GTE flag is non-negative, queues sixteen gouraud `POLY_G4` segments
/// forming a ring between two radii, `arg1` and `arg1 + arg2` in world units
/// scaled by depth. The edge at `arg1` is black and the edge at `arg1 + arg2`
/// carries `rgb`, so the ring fades out towards `arg1`.
static void func_neo_ark_south_promenade_8017D9C4(GfxCoord* arg0, s32 arg1, s32 arg2, u8* rgb)
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

/// Projects the coordinate's world position through `GsWSMATRIX` and, unless
/// the GTE flags the projection, queues eight gouraud `POLY_G4` wedges filling
/// a disc around the projected point, `rgb` at the centre and black at the rim.
/// `arg1` is the radius in world units, scaled by depth.
static void func_neo_ark_south_promenade_8017DDF0(GfxCoord* arg0, s16 arg1, u8* rgb)
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
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        }
    }
    SCRATCH_POP_BYTES(0x18);
}

/// Twin smoke trail. State 0 allocates sixteen `GfxCoord`s, eight per
/// trail, and seeds them all from the two spawn offsets so each trail starts
/// collapsed on its origin. State 1 advances one slot of each trail per frame,
/// re-derives all sixteen against the view and draws them. The task frees
/// itself once its age reaches the spawn argument, and idles while the room's
/// event state is 2 or more.
void func_neo_ark_south_promenade_8017E184(Task* task)
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
    objCoord = task->extra.disp2d->coord;

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
                objCoord->coord.t[0]   = D_neo_ark_south_promenade_8017F6DC[0].vx;
                objCoord->coord.t[1]   = D_neo_ark_south_promenade_8017F6DC[0].vy;
                objCoord->coord.t[2]   = D_neo_ark_south_promenade_8017F6DC[0].vz;
                objCoord->composeStamp = GRAPHICS_COORD_DIRTY;
                Gp_UpdateCoord(objCoord);
                task->state        = 1;
                coord.parent       = work->parent;
                vec                = &D_neo_ark_south_promenade_8017F6DC[1];
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
                    SVECTOR* edge    = &D_neo_ark_south_promenade_8017F6DC[1];
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
                func_neo_ark_south_promenade_8017E674(coords, &coords[8], work->age & 7, 0x123);
                if (work->age == task->spawnArg1.value && work->age != 0) {
                    Gp_ReleaseState1CMem(work, task);
                }
                break;
        }
    }
}

/// Draws the two eight-slot coordinate trails as seven gouraud `POLY_G4`
/// quads, walking backwards from `arg2`. Each quad spans `workm.t` of two
/// adjacent slots on `arg0` and `arg1`. The leading edge is scaled by
/// `0x40 - 9 * i` and the trailing edge by nine less. `arg3` is the beam
/// colour, three 2-bit channels at bits 8, 4 and 0 that each multiply that
/// fade. Dropped when `gte_stflg` is negative.
static void func_neo_ark_south_promenade_8017E674(GfxCoord* arg0, GfxCoord* arg1, s16 arg2, s16 arg3)
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

/// Spark burst. State 0 fires the burst's effect, then a non-zero spawn
/// argument starts a stream of jittered sparks (state 1) and a zero one a pair
/// of rings whose radius grows and brightness falls each frame (state 2).
/// Either way the task reaches state 3 after seven frames and releases its
/// work block, or earlier once the room's event state reaches 4.
void func_neo_ark_south_promenade_8017EA6C(Task* task)
{
    GfxCoord*  objCoord;
    GpEffWork* work;
    u8         rgb[4];

    objCoord = task->extra.disp2d->coord;
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
            func_neo_ark_south_promenade_8017D9C4(objCoord, 0x100, 0x100, rgb);
            func_neo_ark_south_promenade_8017D9C4(objCoord, work->scale, work->scale, rgb);
            if (work->age >= 7) {
                task->state = 3;
            }
            break;

        case 3:
            Gp_ReleaseState1CMem(work, task);
            break;
    }
}

/// Projects the coordinate's world position through `GsWSMATRIX` and, unless
/// the GTE flags the projection, queues a star-shaped glow of gouraud `POLY_G4`
/// wedges: sixteen around the circle, alternating full radius at half
/// intensity and half radius at full intensity, then four spikes a quarter
/// turn apart, two reaching the full radius and two twice it. `arg1` sizes it
/// in world units scaled by depth; every wedge fades to black at its rim.
static void func_neo_ark_south_promenade_8017ECF4(GfxCoord* arg0, s16 arg1, u8* arg2)
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
