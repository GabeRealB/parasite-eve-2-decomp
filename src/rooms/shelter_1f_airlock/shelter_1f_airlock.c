#include "rooms/shelter_1f_airlock.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>

#include "gte.h"
#include "types.h"

#include "actors/task_tables.h"

#include "gameplay/area.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/direction_input.h"
#include "gameplay/light.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gfx.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/task.h"
#include "main/task_types.h"

#include "mapui/map_neo_ark.h"

#include "overlay.h"

#include "rooms/room_common.h"

#define D_shelter_1f_airlock_8017E4C4 (D_shelter_1f_airlock_8017E4BC + 1)
#define D_shelter_1f_airlock_8017E4D4 (D_shelter_1f_airlock_8017E4BC + 3)

/// The room's message table, handed to its event task in state 0.
extern GpMsgEntry D_shelter_1f_airlock_8017E494[];

/// Ambient effect emitter positions for the airlock, selected by view index.
/// `D_shelter_1f_airlock_8017E4BC` / `_8017E4C4` / `_8017E4D4` are successive
/// labels into one contiguous run of `SVECTOR`s, so the per-view lists overlap.

static void func_shelter_1f_airlock_8017D8A8(SVECTOR* arg0, s32 arg1, s32 arg2);
static void func_shelter_1f_airlock_8017E0F0(SVECTOR* arg0, s32 arg1, s32 arg2);

// Indexed views below share one contiguous table.
s32 func_shelter_1f_airlock_8017D5D0(Task*, s32, GpMessageArg, GpMessageArg);
s32 func_shelter_1f_airlock_8017D5D8(Task*, s32, GpSaveLoc*, GpSaveLoc*);
s32 func_shelter_1f_airlock_8017D61C(Task*, s32, GpMessageArg, GpMessageArg);
s32 func_shelter_1f_airlock_8017D624(Task*, s32, GpMessageArg, GpMessageArg);

extern GpGridParams   D_shelter_1f_airlock_8017E838[1];
extern GpObj3A        D_shelter_1f_airlock_8017F7B8[2];
extern GpObj4C        D_shelter_1f_airlock_8017F430[6];
extern GpObj4C        D_shelter_1f_airlock_8017F5F8[4];
extern GpRoomCoordSet D_shelter_1f_airlock_8017F418[1];

GpMsgEntry D_shelter_1f_airlock_8017E494[5] = {
    { 5102, func_shelter_1f_airlock_8017D5D8 },
    { 5105, func_shelter_1f_airlock_8017D5D0 },
    { 5103, func_shelter_1f_airlock_8017D624 },
    { 5104, func_shelter_1f_airlock_8017D61C },
    { 0x7FFFFFFF, NULL },
};

SVECTOR D_shelter_1f_airlock_8017E4BC[28] = {
    { -3000, -2240, 5030, 0 },
    { 0, -2240, 5030, 0 },
    { 2000, -2240, 4100, 0 },
    { -5700, -1340, 3260, 0 },
    { 220, -2010, 4290, 0 },
    { 220, -1760, 4120, 0 },
    { -220, -2010, 4290, 0 },
    { -220, -1760, 4120, 0 },
    { 200, -2010, 4280, 0 },
    { -200, -2010, 4280, 0 },
    { 200, -1770, 4120, 0 },
    { -200, -1770, 4120, 0 },
    { -1210, -2010, 4290, 0 },
    { -1210, -1760, 4120, 0 },
    { -1650, -2010, 4290, 0 },
    { -1650, -1760, 4120, 0 },
    { -1230, -2010, 4280, 0 },
    { -1630, -2010, 4280, 0 },
    { -1230, -1770, 4120, 0 },
    { -1630, -1770, 4120, 0 },
    { -2780, -2010, 4290, 0 },
    { -2780, -1760, 4120, 0 },
    { -3220, -2010, 4290, 0 },
    { -3220, -1760, 4120, 0 },
    { -2800, -2010, 4280, 0 },
    { -3200, -2010, 4280, 0 },
    { -2800, -1770, 4120, 0 },
    { -3200, -1770, 4120, 0 },
};

GpRoomObjRec D_shelter_1f_airlock_8017E59C[1] = {
    { D_shelter_1f_airlock_8017E838, D_shelter_1f_airlock_8017F430, D_shelter_1f_airlock_8017F5F8, D_shelter_1f_airlock_8017F7B8 },
};

GpRoomCoordRec D_shelter_1f_airlock_8017E5AC[1] = {
    { D_shelter_1f_airlock_8017F418, NULL },
};

u8* D_shelter_1f_airlock_8017E5B4[1] = {
    D_8010CAF8,
};

GpViewCountRec D_shelter_1f_airlock_8017E5B8[1] = {
    { { .bytes = { 5, 0 } } },
};

GpWarpRec D_shelter_1f_airlock_8017E5BC[2] = {
    { { .words = { 0, 2051, 0, 3070 } }, { 0, 0, 0, 0 }, { .words = { 0, 2100, 0, 3390 } }, { 0, 0, 0, 0 }, 0x55050002, 0x55050001, 0, 2, 0, 0 },
    { { .words = { 0, -5050, 0, 3070 } }, { 0, 0, 0, 0 }, { .words = { 0, -5050, 0, 3070 } }, { 0, 0, 0, 0 }, 0x55050002, 0x55050001, 0, 5, 0, 0 },
};

SVECTOR D_shelter_1f_airlock_8017E62C[6] = {
#include "assets/shelter_1f_airlock_collision_01278_normals.inc"
};

SVECTOR D_shelter_1f_airlock_8017E65C[32] = {
#include "assets/shelter_1f_airlock_collision_01278_verts.inc"
};

GpGridFace D_shelter_1f_airlock_8017E75C[13] = {
#include "assets/shelter_1f_airlock_collision_01278_faces.inc"
};

s16 D_shelter_1f_airlock_8017E7F8[26] = {
#include "assets/shelter_1f_airlock_collision_01278_cells.inc"
};

#define GRID_CELL(i) (&D_shelter_1f_airlock_8017E7F8[i])
s16* D_shelter_1f_airlock_8017E82C[3] = {
#include "assets/shelter_1f_airlock_collision_01278_table.inc"
};
#undef GRID_CELL

GpGridParams D_shelter_1f_airlock_8017E838[1] = {
    { NULL, D_shelter_1f_airlock_8017E62C, D_shelter_1f_airlock_8017E65C, D_shelter_1f_airlock_8017E75C, D_shelter_1f_airlock_8017E82C, 5750, -2500, 3, 1, 4000, 13 },
};

GpViewRec D_shelter_1f_airlock_8017E85C[5] = {
    { { { { 4095, 0, 0 }, { 0, 0, -4096 }, { 0, 4095, 0 } }, { 0, 0x5334, 0 } }, 207 },
    { { { { -3992, 0, 916 }, { 448, 3570, 1955 }, { -799, 2006, -3480 } }, { -2240, 2240, -5400 } }, 230 },
    { { { { -434, 0, -4072 }, { 315, 4083, -33 }, { 4060, -317, -433 } }, { 3460, 700, -5100 } }, 257 },
    { { { { -390, 0, 4077 }, { 1014, 3967, 97 }, { -3949, 1019, -378 } }, { -1540, 2030, -5100 } }, 257 },
    { { { { -4004, 0, 860 }, { 491, 3363, 2285 }, { -706, 2337, -3288 } }, { 4160, 2340, -5400 } }, 207 },
};

GpSprtCmd D_shelter_1f_airlock_8017E910[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtCmd D_shelter_1f_airlock_8017E920[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_shelter_1f_airlock_8017E930[24] = {
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 24, -96, 1150, { .fields = { 48, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 24, -48, 1163, { .fields = { 24, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 24, -32, 1201, { .fields = { 40, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 24, 0, 1196, { .fields = { 24, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 24, 24, 1143, { .fields = { 48, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 24, 32, 1157, { .fields = { 80, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 24, 64, 1157, { .fields = { 40, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 64 } }, 40, -96, 1100, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 64 } }, 64, -96, 950, { .fields = { 96, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 64 } }, 96, -96, 875, { .fields = { 96, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 64 } }, 128, -96, 875, { .fields = { 96, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 56 } }, 128, -32, 875, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, 128, 24, 875, { .fields = { 16, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 56 } }, 96, -32, 875, { .fields = { 64, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, 96, 24, 875, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 56 } }, 64, -32, 950, { .fields = { 40, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, 64, 24, 950, { .fields = { 32, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 56 } }, 40, -32, 1100, { .fields = { 72, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 40, 24, 1100, { .fields = { 8, 48 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 32, 56 } }, 128, 64, 875, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 32, 56 } }, 96, 64, 875, { .fields = { 64, 56 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 48 } }, 72, 64, 950, { .fields = { 16, 104 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 32 } }, 48, 64, 1100, { .fields = { 56, 224 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 24 } }, 32, 56, 1157, { .fields = { 8, 216 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_shelter_1f_airlock_8017EB10[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 19, 0, 0, { 1, 0 } },
    { 19, 5, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpDrawAreaRec D_shelter_1f_airlock_8017EB30[2] = {
    { { 117, 161, 0, 0 }, 0x4E20 },
    { { 0, 0, 0, 0 }, 0xFFFF },
};

GpSprtElem D_shelter_1f_airlock_8017EB44[26] = {
    { 143, 0x3FC0, { .fields = { 40, 56 } }, -160, -88, 1275, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 56 } }, -120, -88, 1275, { .fields = { 56, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 56 } }, -88, -88, 1275, { .fields = { 72, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -32, -88, 1064, { .fields = { 72, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -32, -72, 1146, { .fields = { 40, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -32, -40, 1430, { .fields = { 32, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -32, -48, 1425, { .fields = { 32, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 56 } }, -56, -88, 1275, { .fields = { 72, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -32, -32, 1418, { .fields = { 32, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -32, -24, 1411, { .fields = { 48, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -32, -16, 1451, { .fields = { 64, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -32, -8, 1448, { .fields = { 80, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -32, 0, 1473, { .fields = { 96, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -32, 8, 1498, { .fields = { 32, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -32, 24, 1498, { .fields = { 112, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 56 } }, -56, -32, 1275, { .fields = { 104, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 64 } }, -160, -32, 1275, { .fields = { 88, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 64 } }, -120, -32, 1275, { .fields = { 96, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 64 } }, -88, -32, 1275, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -56, 24, 1275, { .fields = { 72, 184 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, -32, 16, 1498, { .fields = { 48, 88 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, -40, 24, 1250, { .fields = { 48, 40 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 40 } }, -56, 24, 1225, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 48 } }, -80, 24, 1225, { .fields = { 48, 160 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 32, 48 } }, -112, 24, 1225, { .fields = { 40, 112 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 48, 40 } }, -160, 24, 1225, { .fields = { 24, 208 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_shelter_1f_airlock_8017ED4C[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 20, 0, 0, { 1, 0 } },
    { 20, 6, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpDrawAreaRec D_shelter_1f_airlock_8017ED6C[2] = {
    { { 149, 229, 0, 0 }, 0x343A },
    { { 0, 0, 0, 0 }, 0xFFFF },
};

GpSprtElem D_shelter_1f_airlock_8017ED80[36] = {
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -72, -120, 311, { .fields = { 96, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -72, -80, 311, { .fields = { 96, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -72, 0, 435, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -40, 16, 434, { .fields = { 120, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -40, -16, 422, { .fields = { 120, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -40, -88, 320, { .fields = { 96, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -40, -120, 313, { .fields = { 120, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -32, -120, 338, { .fields = { 80, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -32, -104, 334, { .fields = { 80, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -32, -88, 342, { .fields = { 96, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -40, -64, 313, { .fields = { 88, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -32, -64, 333, { .fields = { 88, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -32, -40, 323, { .fields = { 80, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -32, -24, 341, { .fields = { 104, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -32, 0, 425, { .fields = { 104, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -32, 24, 432, { .fields = { 112, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -24, 48, 434, { .fields = { 104, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -56, 40, 422, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, -72, 56, 519, { .fields = { 88, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -72, 40, 475, { .fields = { 72, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -72, -40, 307, { .fields = { 56, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, -72, -24, 313, { .fields = { 72, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -40, 56, 521, { .fields = { 72, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -40, 64, 538, { .fields = { 80, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -40, 48, 425, { .fields = { 80, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -40, 72, 540, { .fields = { 80, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -48, 80, 543, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -72, 96, 589, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -72, 80, 560, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -24, 88, 538, { .fields = { 112, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -24, 72, 546, { .fields = { 88, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -40, 80, 544, { .fields = { 72, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -40, 88, 549, { .fields = { 72, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -40, 96, 547, { .fields = { 80, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -40, -48, 322, { .fields = { 88, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -40, -32, 308, { .fields = { 88, 32 } }, 128, 128, 128, 0 },
};

GpSprtCmd D_shelter_1f_airlock_8017F050[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 36, 0, 0, { 0, 0 } },
    { 0xFFFF, 0, 0, 0, { 0, 0 } },
};

GpDrawAreaRec D_shelter_1f_airlock_8017F068[2] = {
    { { 87, 207, 0, 0 }, 0x2BF2 },
    { { 0, 0, 0, 0 }, 0xFFFF },
};

GpSprtRec D_shelter_1f_airlock_8017F07C[5] = {
    { { .empty = D_shelter_1f_airlock_8017E910 }, D_shelter_1f_airlock_8017E910, NULL },
    { { .empty = D_shelter_1f_airlock_8017E920 }, D_shelter_1f_airlock_8017E920, NULL },
    { { .elements = D_shelter_1f_airlock_8017E930 }, D_shelter_1f_airlock_8017EB10, D_shelter_1f_airlock_8017EB30 },
    { { .elements = D_shelter_1f_airlock_8017EB44 }, D_shelter_1f_airlock_8017ED4C, D_shelter_1f_airlock_8017ED6C },
    { { .elements = D_shelter_1f_airlock_8017ED80 }, D_shelter_1f_airlock_8017F050, D_shelter_1f_airlock_8017F068 },
};

GpPointLight D_shelter_1f_airlock_8017F0B8[9] = {
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 2000, -2000, 3500 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2457, 2048, 1638, { 0, 0 } }, 2000, 3000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 10, -1730, 4260 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 1638, 3276, 3276, { 0, 0 } }, 1000, 2000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -1430, -1730, 4260 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 1638, 3276, 3276, { 0, 0 } }, 1000, 2000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -3000, -1730, 4260 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 1638, 3276, 3276, { 0, 0 } }, 1000, 2000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, -2000, 4650 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2457, 2048, 1638, { 0, 0 } }, 2000, 3000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -3000, -2000, 4650 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2457, 2048, 1638, { 0, 0 } }, 2000, 3000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -5000, -2000, 3500 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2457, 2048, 1638, { 0, 0 } }, 2000, 3000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -5580, -1340, 3255 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 4096, 409, 409, { 0, 0 } }, 500, 1500 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -1010, -2099, 4150 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 1228, 4096, 2457, { 0, 0 } }, 100, 200 },
};

GpRoomCoordSet D_shelter_1f_airlock_8017F418[1] = {
    { 0, NULL, 9, D_shelter_1f_airlock_8017F0B8, 0, NULL },
};

GpObj4C D_shelter_1f_airlock_8017F430[6] = {
    { NULL, NULL, NULL, { 2083, -1808, 4063, 0 }, { { -1404, -2271, -163, 0 }, { 1400, -2271, 159, 0 }, { -1404, 2272, -163, 0 }, { 1400, 2272, 159, 0 } }, { 467, 0, -4078, 0 }, { 0, 0, 4096, 0 }, 2672, 0, 2, 3, 1, 0 },
    { NULL, NULL, NULL, { 2130, -1856, 3839, 0 }, { { 1444, -2256, 160, 0 }, { -1461, -2256, -177, 0 }, { 1444, 2256, 160, 0 }, { -1461, 2256, -177, 0 } }, { -474, 0, 4071, 0 }, { 0, 0, 4096, 0 }, 2684, 0, 3, 2, 1, 0 },
    { NULL, NULL, NULL, { -993, -1408, 4785, 0 }, { { -432, -2656, 1520, 0 }, { 432, -2656, -1520, 0 }, { -432, 2656, 1520, 0 }, { 432, 2656, -1520, 0 } }, { -3943, 0, -1121, 0 }, { 0, 0, 4096, 0 }, 3082, 0, 4, 3, 1, 0 },
    { NULL, NULL, NULL, { -1169, -1409, 4785, 0 }, { { 448, -2624, -1536, 0 }, { -448, -2624, 1536, 0 }, { 448, 2624, -1536, 0 }, { -448, 2624, 1536, 0 } }, { 3936, 0, 1148, 0 }, { 0, 0, 4096, 0 }, 3072, 0, 3, 4, 1, 0 },
    { NULL, NULL, NULL, { -5377, -1456, 3969, 0 }, { { 1573, -2128, -267, 0 }, { -1586, -2128, 235, 0 }, { 1573, 2128, -267, 0 }, { -1586, 2128, 235, 0 } }, { 643, 0, 4051, 0 }, { 0, 0, 4096, 0 }, 2660, 0, 4, 5, 1, 0 },
    { NULL, NULL, NULL, { -5248, -1376, 4160, 0 }, { { -1571, -2080, 234, 0 }, { 1544, -2080, -281, 0 }, { -1571, 2080, 234, 0 }, { 1544, 2080, -281, 0 } }, { -672, 0, -4053, 0 }, { 0, 0, 4096, 0 }, 2610, 0, 5, 4, 129, 0 },
};

GpObj4C D_shelter_1f_airlock_8017F5F8[4] = {
    { NULL, NULL, NULL, { 1984, -48, 2912, 0 }, { { -608, 0, -352, 0 }, { 608, 0, -352, 0 }, { -608, 0, 352, 0 }, { 608, 0, 352, 0 } }, { 0, 4103, 0, 0 }, { 0, 0, 4096, 0 }, 701, 0, 1, 18, 2, 0 },
    { NULL, NULL, NULL, { -4960, -48, 2912, 0 }, { { -608, 0, -384, 0 }, { 608, 0, -384, 0 }, { -608, 0, 384, 0 }, { 608, 0, 384, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, 4096, 0 }, 718, 0, 2, 33, 2, 0 },
    { NULL, NULL, NULL, { -5360, -64, 3280, 0 }, { { -336, 0, -560, 0 }, { 336, 0, -560, 0 }, { -336, 0, 560, 0 }, { 336, 0, 560, 0 } }, { 0, 4107, 0, 0 }, { 4096, 0, 0, 0 }, 652, 2, 1, 0, 2, 0 },
    { NULL, NULL, NULL, { -1376, -64, 4192, 0 }, { { -608, 0, -352, 0 }, { 608, 0, -352, 0 }, { -608, 0, 352, 0 }, { 608, 0, 352, 0 } }, { 0, 4103, 0, 0 }, { 0, 0, 4096, 0 }, 701, 2, 2, 0, 130, 0 },
};

GpAreaTmdRec D_shelter_1f_airlock_8017F728[2] = {
    { 22, 22, 3, 0, { 0, 0 }, D_80154188 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_shelter_1f_airlock_8017F740[2] = {
    { 39, 39, 3, 0, { 0, 0 }, D_801540E0 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaVariant D_shelter_1f_airlock_8017F758[12] = {
    { NULL, NULL },
    { D_map_neo_ark_8017AFD0, D_shelter_1f_airlock_8017F728 },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { D_map_neo_ark_8017AFF0, D_shelter_1f_airlock_8017F740 },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
};

GpObj3A D_shelter_1f_airlock_8017F7B8[2] = {
    { NULL, NULL, { -64, -1264, 3072, 0 }, { { -1216, 2480, -832, 0 }, { 1216, 2480, 832, 0 }, { -1216, -2480, -832, 0 }, { 1216, -2480, 832, 0 } }, { -2315, 0, 3382, 0 }, { 57, 11 }, 1, 0 },
    { NULL, NULL, { -2801, -1440, 3071, 0 }, { { -1433, 2480, 833, 0 }, { 1434, 2480, -832, 0 }, { -1433, -2480, 833, 0 }, { 1434, -2480, -832, 0 } }, { 2060, 0, 3547, 0 }, { -98, 11 }, 129, 0 },
};

s32 D_shelter_1f_airlock_8017F830[3] = {
    0x10000059,
    0x1000005B,
    0x10000059,
};

GpRoomParamRec D_shelter_1f_airlock_8017F83C[1] = {
    { 0, 0, 1, 0, NULL },
};

GpRoomParamRec D_shelter_1f_airlock_8017F844[1] = {
    { 0, 0, 1, 0, D_shelter_1f_airlock_8017F830 },
};

GpRoomParamRec* D_shelter_1f_airlock_8017F84C[8] = {
    D_shelter_1f_airlock_8017F83C,
    D_shelter_1f_airlock_8017F844,
    D_shelter_1f_airlock_8017F83C,
    D_shelter_1f_airlock_8017F83C,
    D_shelter_1f_airlock_8017F83C,
    D_shelter_1f_airlock_8017F83C,
    D_shelter_1f_airlock_8017F83C,
    D_shelter_1f_airlock_8017F83C,
};

static void func_shelter_1f_airlock_8017D62C(Task* task);
static void func_shelter_1f_airlock_8017D670(Task* task);

s32 func_shelter_1f_airlock_8017D5D0(Task* task, s32 msgId, GpMessageArg arg2, GpMessageArg arg3)
{
    return 0;
}

/// The room's handler for message 0x13EE: copies the incoming save location
/// onto the outgoing one, passes both to `func_map_neo_ark_80179B14` and returns 1.
s32 func_shelter_1f_airlock_8017D5D8(Task* arg0, s32 arg1, GpSaveLoc* in, GpSaveLoc* out)
{
    *out = *in;
    func_map_neo_ark_80179B14(in, out);
    return 1;
}

s32 func_shelter_1f_airlock_8017D61C(Task* task, s32 msgId, GpMessageArg arg2, GpMessageArg arg3)
{
    return 0;
}

s32 func_shelter_1f_airlock_8017D624(Task* task, s32 msgId, GpMessageArg arg2, GpMessageArg arg3)
{
    return 0;
}

/// State 0 of the room's event task: installs the room's message table,
/// publishes the task in pointer slot 7 and advances to state 1.
static void func_shelter_1f_airlock_8017D62C(Task* task)
{
    task->msgTable = D_shelter_1f_airlock_8017E494;
    Game_SetPtrSlot(task, 7);
    task->state = (s32)(task->state + 1);
}

/// State 1 of the room's event task: does nothing, so the task idles here.
static void func_shelter_1f_airlock_8017D670(Task* task)
{
}

/// The event task's three states: install the message table, idle, and kill.
static const TaskFuncTable3 D_shelter_1f_airlock_8017D5C4 = {
    {
        func_shelter_1f_airlock_8017D62C,
        func_shelter_1f_airlock_8017D670,
        taskKill,
    },
};

/// The room's event task: runs the handler for its current state, through a
/// stack copy of the state table.
void func_shelter_1f_airlock_8017D678(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_shelter_1f_airlock_8017D5C4;
    sp.funcs[task->state](task);
}

void func_shelter_1f_airlock_8017D6D0(Task* unused)
{
    switch (Gp_GetViewIndex() & 0xFF) {
        case 3:
            func_shelter_1f_airlock_8017E0F0(&D_shelter_1f_airlock_8017E4C4[0], 0x200, 0x111);
            func_shelter_1f_airlock_8017E0F0(&D_shelter_1f_airlock_8017E4C4[1], 0x200, 0x111);
            func_shelter_1f_airlock_8017D8A8(&D_shelter_1f_airlock_8017E4C4[3], 0x180, 0x1011);
            func_shelter_1f_airlock_8017D8A8(&D_shelter_1f_airlock_8017E4C4[5], 0x180, 0x1011);
            func_shelter_1f_airlock_8017D8A8(&D_shelter_1f_airlock_8017E4C4[7], 0x180, 0x1011);
            func_shelter_1f_airlock_8017D8A8(&D_shelter_1f_airlock_8017E4C4[9], 0x180, 0x1011);
            func_shelter_1f_airlock_8017D8A8(&D_shelter_1f_airlock_8017E4C4[11], 0x180, 0x1011);
            func_shelter_1f_airlock_8017D8A8(&D_shelter_1f_airlock_8017E4C4[17], 0x180, 0x1011);
            break;
        case 4:
            func_shelter_1f_airlock_8017E0F0(&D_shelter_1f_airlock_8017E4BC[0], 0x200, 0x111);
            func_shelter_1f_airlock_8017E0F0(&D_shelter_1f_airlock_8017E4BC[1], 0x200, 0x111);
            func_shelter_1f_airlock_8017D8A8(&D_shelter_1f_airlock_8017E4BC[4], 0x180, 0x1011);
            func_shelter_1f_airlock_8017D8A8(&D_shelter_1f_airlock_8017E4BC[6], 0x180, 0x1011);
            func_shelter_1f_airlock_8017D8A8(&D_shelter_1f_airlock_8017E4BC[8], 0x180, 0x1011);
            func_shelter_1f_airlock_8017D8A8(&D_shelter_1f_airlock_8017E4BC[10], 0x180, 0x1011);
            func_shelter_1f_airlock_8017D8A8(&D_shelter_1f_airlock_8017E4BC[12], 0x180, 0x1011);
            func_shelter_1f_airlock_8017D8A8(&D_shelter_1f_airlock_8017E4BC[14], 0x180, 0x1011);
            func_shelter_1f_airlock_8017D8A8(&D_shelter_1f_airlock_8017E4BC[16], 0x180, 0x1011);
            func_shelter_1f_airlock_8017D8A8(&D_shelter_1f_airlock_8017E4BC[18], 0x180, 0x1011);
            func_shelter_1f_airlock_8017D8A8(&D_shelter_1f_airlock_8017E4BC[20], 0x180, 0x1011);
            func_shelter_1f_airlock_8017D8A8(&D_shelter_1f_airlock_8017E4BC[22], 0x180, 0x1011);
            func_shelter_1f_airlock_8017D8A8(&D_shelter_1f_airlock_8017E4BC[24], 0x180, 0x1011);
            func_shelter_1f_airlock_8017D8A8(&D_shelter_1f_airlock_8017E4BC[26], 0x180, 0x1011);
            break;
        case 5:
            func_shelter_1f_airlock_8017E0F0(&D_shelter_1f_airlock_8017E4D4[0], 0x200, 0x200);
            break;
    }
}

/// Projects two adjacent positions and draws a colored glow between them.
/// The RGB nibbles in arg2 gain an alternating frame contribution whose shift
/// is selected by bits 12..15. Uses the two-point `OverlayPointPairScratch` layout.
static void func_shelter_1f_airlock_8017D8A8(SVECTOR* arg0, s32 arg1, s32 arg2)
{
    void**                   scratch;
    u8*                      head;
    OverlayPointPairScratch* block;
    POLY_G4*                 prim;
    DisplayState*            ds;
    SVECTOR*                 p1;
    s32                      ang;
    s32                      t;
    s32                      conn;
    s32                      t3;
    s32                      t2;
    s32                      limit;
    s32                      angStart;
    s32                      packed;
    s32                      blend;
    s32                      tr;
    s32                      tg;
    s32                      scaled;
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
            blend     = ds->animFrame;
            packed    = arg2 << 16;
            blend     = blend & 1;
            ang       = (s16)ang;
            blend     = blend << (packed >> 28);
            tr        = (packed >> 20) & 0xF0;
            tg        = (packed >> 16) & 0xF0;
            r         = blend + tr;
            g         = blend + tg;
            b         = blend + ((arg2 & 0xF) << 4);
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

/// Draws a glow at a world-space point: projects `arg0` through
/// `gGfxViewCoord.workm` and, when the resulting OTZ is at least 0x11, queues
/// four gouraud `POLY_G4` wedges around the projected centre, dark at the rim
/// and coloured at the centre. The on-screen radius is `(s16)arg1 * 64 / otz`.
/// The centre colour takes red from bits 8..15 of `arg2` and green and blue
/// from two-bit fields at bits 4 and 0, each scaled by a brightness that
/// alternates with the frame counter.
static void func_shelter_1f_airlock_8017E0F0(SVECTOR* arg0, s32 arg1, s32 arg2)
{
    u8*                head;
    RoomDraw25Scratch* block;
    POLY_G4*           prim;
    u8*                ds_ptr;
    DisplayState*      ds;
    s32                radius;
    s32                ang;
    s32                t;
    s32                t2;
    s32                packed;
    u8                 blend;
    u8                 r;
    u8                 g;
    u8                 b;

    {
        void** scratch;
        u8*    tmp;

        scratch = (void**)G_SCRATCH_HEAD;
        head    = *scratch;
        tmp     = (*scratch = head - 0xC);
        block   = (RoomDraw25Scratch*)tmp;
    }

    gte_SetTransMatrix(&gGfxViewCoord.workm);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_ldv0(arg0);
    gte_rtps();
    gte_stsxy(&((RoomDraw25Scratch*)(head - 0xC))->sx);
    gte_stszotz(&block->otz);
    if (((RoomDraw25Scratch*)(head - 0xC))->otz >= 0x11) {
        radius        = ((s16)arg1 * 64) / ((RoomDraw25Scratch*)(head - 0xC))->otz;
        ds_ptr        = (u8*)&gDisplayState;
        packed        = arg2 << 16;
        blend         = (((u8)((DisplayState*)ds_ptr)->animFrame & 1) * 8) | 0x20;
        r             = blend * (packed >> 24);
        g             = blend * ((packed >> 20) & 3);
        b             = blend * (arg2 & 3);
        ang           = 0;
        ds            = (DisplayState*)ds_ptr;
        block->radius = radius;
        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
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
            addPrim(Gpu_OtEntryAtByteOffset(((((u32)block->otz << ds->otDepthShift) >> 2) & 0xFFC)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        } while (ang < 0x1000);
    }
    SCRATCH_POP_BYTES(0xC);
}
