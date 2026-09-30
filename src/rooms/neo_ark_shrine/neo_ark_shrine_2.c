#include "rooms/neo_ark_shrine.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "common.h"
#include "gte.h"

#include "neo_ark_shrine_private.h"

#include "actors/task_tables.h"

#include "gameplay/display.h"
#include "gameplay/action_prompt.h"
#include "gameplay/actor_render.h"
#include "gameplay/area.h"
#include "gameplay/captions.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/direction_input.h"
#include "gameplay/effects.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/item_menu.h"
#include "gameplay/light.h"
#include "gameplay/loading.h"
#include "gameplay/pad_script.h"
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
#include "main/mem.h"
#include "main/pad.h"
#include "main/pad_types.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "mapui/map_neo_ark.h"

#include "overlay.h"

#include "rooms/room_common.h"

/// Scratch state of the two falling-prop tasks, stored at `Task::work`
/// (`memCalloc(0x48)` in `func_neo_ark_shrine_8017F4C8` / `_8017F688`).
/// `color` / `light` are the prop's own matrices, republished onto
/// `TmdObject::lightMtx` / `colorMtx` by the two spawn handlers; `speed` /
/// `delta` / `ticks` are the fall itself, stepped by `func_neo_ark_shrine_8017F578`.
typedef struct {
    /* 0x00 */ MATRIX color;
    /* 0x20 */ MATRIX light;
    /* 0x40 */ u16    speed; ///< per-frame gravity step
    /* 0x42 */ u16    delta; ///< accumulated fall distance for this frame
    /* 0x44 */ u16    ticks; ///< frames since the fall started
    /* 0x46 */ u8     pad_46[2];
} NeoArkShrineFall;

static void func_neo_ark_shrine_8017E988(s32 x, s32 y, s32 variant);
static void func_neo_ark_shrine_8017F80C(Task* task);
static void func_neo_ark_shrine_8017F86C(Task* task);
static void func_neo_ark_shrine_8017FC14(SVECTOR* pos, s32 arg1, s32 arg2);
static void func_neo_ark_shrine_80180144(GfxCoord* arg0, s32 arg1, s32 arg2, u8* rgb);
static void func_neo_ark_shrine_80180570(GfxCoord* arg0, s16 arg1, u8* rgb);
static void func_neo_ark_shrine_80180DF4(GfxCoord* arg0, GfxCoord* arg1, s16 arg2, s16 arg3);
static void func_neo_ark_shrine_80181474(GfxCoord* arg0, s16 arg1, u8* arg2);

extern SVECTOR D_neo_ark_shrine_8018268C[];
extern SVECTOR D_neo_ark_shrine_80182694[];
extern SVECTOR D_neo_ark_shrine_8018269C[];
extern SVECTOR D_neo_ark_shrine_801826AC[];
extern SVECTOR D_neo_ark_shrine_801826C4[];
extern SVECTOR D_neo_ark_shrine_801826D4[];
extern SVECTOR D_neo_ark_shrine_80182704[];

/// Offset of the beam's near end from the effect's parent coordinate. The far
/// end's offset follows it directly; the beam's set-up state reaches that one
/// as element 1.

/// Offset of the beam's far end from the effect's parent coordinate.

static void func_neo_ark_shrine_8017ECC4(Task* task);
static void func_neo_ark_shrine_8017EDAC(Task* task);
static void func_neo_ark_shrine_8017EDE0(Task* task);
static void func_neo_ark_shrine_8017EE44(Task* task);
static void func_neo_ark_shrine_8017EED4(Task* task);
static void func_neo_ark_shrine_8017EF68(Task* task);
static void func_neo_ark_shrine_8017EFE4(Task* task);
static void func_neo_ark_shrine_8017F094(Task* task);
static void func_neo_ark_shrine_8017F0F0(Task* task);
static void func_neo_ark_shrine_8017F178(Task* task);
static void func_neo_ark_shrine_8017F21C(Task* task);
static void func_neo_ark_shrine_8017F274(Task* task);
static void func_neo_ark_shrine_8017F320(Task* task);
static void func_neo_ark_shrine_8017F398(Task* task);
static void func_neo_ark_shrine_8017F4C8(Task* task);
static void func_neo_ark_shrine_8017F578(Task* task);
static void func_neo_ark_shrine_8017F640(Task* task);
static void func_neo_ark_shrine_8017F688(Task* task);
static void func_neo_ark_shrine_8017F738(Task* task);

/// State table of the shrine's cap script task, indexed by `Task::state`.
static const TaskFuncTable16 D_neo_ark_shrine_8017D5D0 = {
    {
        func_neo_ark_shrine_8017ECC4,
        func_neo_ark_shrine_8017EDAC,
        func_neo_ark_shrine_8017D9A0,
        func_neo_ark_shrine_8017EDE0,
        func_neo_ark_shrine_8017EE44,
        func_neo_ark_shrine_8017EED4,
        func_neo_ark_shrine_8017DB10,
        func_neo_ark_shrine_8017EF68,
        func_neo_ark_shrine_8017EFE4,
        func_neo_ark_shrine_8017F094,
        func_neo_ark_shrine_8017F0F0,
        func_neo_ark_shrine_8017F178,
        func_neo_ark_shrine_8017F21C,
        func_neo_ark_shrine_8017F274,
        func_neo_ark_shrine_8017F320,
        func_neo_ark_shrine_8017F398,
    },
};
/// State table of the shrine's first falling prop, indexed by `Task::state`.
static const TaskFuncTable4 D_neo_ark_shrine_8017D610 = {
    { func_neo_ark_shrine_8017F4C8, func_neo_ark_shrine_8017F578, func_neo_ark_shrine_8017F640, taskKill },
};
/// State table of the shrine's second falling prop, indexed by `Task::state`.
static const TaskFuncTable3 D_neo_ark_shrine_8017D620 = {
    { func_neo_ark_shrine_8017F688, func_neo_ark_shrine_8017F738, taskKill },
};

extern GpGridParams   D_neo_ark_shrine_80182D2C[1];
extern GpGridParams   D_neo_ark_shrine_801831D8[1];
extern GpGridParams   D_neo_ark_shrine_80183698[1];
extern GpObj3A        D_neo_ark_shrine_80186730[4];
extern GpObj4C        D_neo_ark_shrine_80185A80[14];
extern GpObj4C        D_neo_ark_shrine_80185EA8[9];
extern GpObj4C        D_neo_ark_shrine_80186154[8];
extern GpObj4C        D_neo_ark_shrine_801863B4[8];
extern GpRoomCoordSet D_neo_ark_shrine_80185A68[1];

SVECTOR D_neo_ark_shrine_8018268C[1] = {
    { 3560, -1200, 6310, 0 },
};

SVECTOR D_neo_ark_shrine_80182694[1] = {
    { 4990, -1200, 6280, 0 },
};

SVECTOR D_neo_ark_shrine_8018269C[2] = {
    { 3560, -1200, 3690, 0 },
    { 4920, -1200, 3690, 0 },
};

SVECTOR D_neo_ark_shrine_801826AC[3] = {
    { 5770, -1200, 2980, 0 },
    { 5790, -1200, 1740, 0 },
    { 5780, -1200, 440, 0 },
};

SVECTOR D_neo_ark_shrine_801826C4[2] = {
    { 8320, -1200, 5070, 0 },
    { 8340, -1200, 3610, 0 },
};

SVECTOR D_neo_ark_shrine_801826D4[6] = {
    { 9060, -1200, 2750, 0 },
    { 0x28BE, -1200, 2880, 0 },
    { 0x2DBE, -1200, 2750, 0 },
    { 9060, -1200, 260, 0 },
    { 0x28AA, -1200, 260, 0 },
    { 0x2DBE, -1200, 260, 0 },
};

SVECTOR D_neo_ark_shrine_80182704[2] = {
    { 6300, -1870, -4490, 0 },
    { 7760, -1880, -4490, 0 },
};

SVECTOR D_neo_ark_shrine_80182714[2] = {
    { 0, 190, -15, 0 },
    { 0, 1085, 180, 0 },
};

GpRoomObjRec D_neo_ark_shrine_80182724[6] = {
    { D_neo_ark_shrine_80182D2C, D_neo_ark_shrine_80185A80, D_neo_ark_shrine_80185EA8, D_neo_ark_shrine_80186730 },
    { D_neo_ark_shrine_801831D8, D_neo_ark_shrine_80185A80, D_neo_ark_shrine_80186154, D_neo_ark_shrine_80186730 },
    { D_neo_ark_shrine_80183698, D_neo_ark_shrine_80185A80, D_neo_ark_shrine_801863B4, D_neo_ark_shrine_80186730 },
    { D_neo_ark_shrine_80182D2C, D_neo_ark_shrine_80185A80, D_neo_ark_shrine_80185EA8, D_neo_ark_shrine_80186730 },
    { D_neo_ark_shrine_801831D8, D_neo_ark_shrine_80185A80, D_neo_ark_shrine_80186154, D_neo_ark_shrine_80186730 },
    { D_neo_ark_shrine_80183698, D_neo_ark_shrine_80185A80, D_neo_ark_shrine_801863B4, D_neo_ark_shrine_80186730 },
};

GpRoomCoordRec D_neo_ark_shrine_80182784[6] = {
    { D_neo_ark_shrine_80185A68, NULL },
    { D_neo_ark_shrine_80185A68, NULL },
    { D_neo_ark_shrine_80185A68, NULL },
    { D_neo_ark_shrine_80185A68, NULL },
    { D_neo_ark_shrine_80185A68, NULL },
    { D_neo_ark_shrine_80185A68, NULL },
};

u8 D_neo_ark_shrine_801827B4[20] = {
    1,
    2,
    3,
    16,
    18,
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
    0,
    0,
};

u8 D_neo_ark_shrine_801827C8[20] = {
    1,
    2,
    3,
    4,
    5,
    6,
    7,
    8,
    9,
    17,
    11,
    12,
    13,
    14,
    15,
    16,
    10,
    18,
    0,
    0,
};

u8 D_neo_ark_shrine_801827DC[20] = {
    1,
    2,
    3,
    16,
    18,
    6,
    7,
    8,
    9,
    17,
    11,
    12,
    13,
    14,
    15,
    16,
    10,
    18,
    0,
    0,
};

u8* D_neo_ark_shrine_801827F0[6] = {
    D_8010CAF8,
    D_neo_ark_shrine_801827B4,
    D_8010CAF8,
    D_neo_ark_shrine_801827C8,
    D_neo_ark_shrine_801827DC,
    D_neo_ark_shrine_801827C8,
};

GpViewCountRec D_neo_ark_shrine_80182808[6] = {
    { { .bytes = { 18, 0 } } },
    { { .bytes = { 18, 0 } } },
    { { .bytes = { 18, 0 } } },
    { { .bytes = { 18, 0 } } },
    { { .bytes = { 18, 0 } } },
    { { .bytes = { 18, 0 } } },
};

GpWarpRec D_neo_ark_shrine_80182814[3] = {
    { { .words = { 1024, 525, 0, 1200 } }, { 0, 0, 0, 0 }, { .words = { 1024, 525, 0, 1200 } }, { 0, 0, 0, 0 }, 0x55150004, 0x55150003, 0x55150005, 9, 0, 440 },
    { { .words = { 3072, 0x32C8, 0, 1530 } }, { 0, 0, 0, 0 }, { .words = { 3072, 0x32C8, 0, 1530 } }, { 0, 0, 0, 0 }, 0x55150002, 0x55150001, 0, 2, 0, 0 },
    { { .words = { 2048, 6950, 0, -3700 } }, { 0, 0, 0, 0 }, { .words = { 2048, 6950, 0, -3700 } }, { 0, 0, 0, 0 }, 0, 0, 0, 10, 0, 0 },
};

SVECTOR D_neo_ark_shrine_801828BC[10] = {
#include "assets/neo_ark_shrine_collision_0576C_normals.inc"
};

SVECTOR D_neo_ark_shrine_8018290C[46] = {
#include "assets/neo_ark_shrine_collision_0576C_verts.inc"
};

GpGridFace D_neo_ark_shrine_80182A7C[30] = {
#include "assets/neo_ark_shrine_collision_0576C_faces.inc"
};

s16 D_neo_ark_shrine_80182BE4[140] = {
#include "assets/neo_ark_shrine_collision_0576C_cells.inc"
};

#define GRID_CELL(i) (&D_neo_ark_shrine_80182BE4[i])
s16* D_neo_ark_shrine_80182CFC[12] = {
#include "assets/neo_ark_shrine_collision_0576C_table.inc"
};
#undef GRID_CELL

GpGridParams D_neo_ark_shrine_80182D2C[1] = {
    { NULL, D_neo_ark_shrine_801828BC, D_neo_ark_shrine_8018290C, D_neo_ark_shrine_80182A7C, D_neo_ark_shrine_80182CFC, 0, 5000, 4, 3, 4000, 30 },
};

SVECTOR D_neo_ark_shrine_80182D50[9] = {
#include "assets/neo_ark_shrine_collision_05C18_normals.inc"
};

SVECTOR D_neo_ark_shrine_80182D98[48] = {
#include "assets/neo_ark_shrine_collision_05C18_verts.inc"
};

GpGridFace D_neo_ark_shrine_80182F18[31] = {
#include "assets/neo_ark_shrine_collision_05C18_faces.inc"
};

s16 D_neo_ark_shrine_8018308C[142] = {
#include "assets/neo_ark_shrine_collision_05C18_cells.inc"
};

#define GRID_CELL(i) (&D_neo_ark_shrine_8018308C[i])
s16* D_neo_ark_shrine_801831A8[12] = {
#include "assets/neo_ark_shrine_collision_05C18_table.inc"
};
#undef GRID_CELL

GpGridParams D_neo_ark_shrine_801831D8[1] = {
    { NULL, D_neo_ark_shrine_80182D50, D_neo_ark_shrine_80182D98, D_neo_ark_shrine_80182F18, D_neo_ark_shrine_801831A8, 0, 5000, 4, 3, 4000, 31 },
};

SVECTOR D_neo_ark_shrine_801831FC[9] = {
#include "assets/neo_ark_shrine_collision_060D8_normals.inc"
};

SVECTOR D_neo_ark_shrine_80183244[50] = {
#include "assets/neo_ark_shrine_collision_060D8_verts.inc"
};

GpGridFace D_neo_ark_shrine_801833D4[31] = {
#include "assets/neo_ark_shrine_collision_060D8_faces.inc"
};

s16 D_neo_ark_shrine_80183548[144] = {
#include "assets/neo_ark_shrine_collision_060D8_cells.inc"
};

#define GRID_CELL(i) (&D_neo_ark_shrine_80183548[i])
s16* D_neo_ark_shrine_80183668[12] = {
#include "assets/neo_ark_shrine_collision_060D8_table.inc"
};
#undef GRID_CELL

GpGridParams D_neo_ark_shrine_80183698[1] = {
    { NULL, D_neo_ark_shrine_801831FC, D_neo_ark_shrine_80183244, D_neo_ark_shrine_801833D4, D_neo_ark_shrine_80183668, 0, 5000, 4, 3, 4000, 31 },
};

GpViewRec D_neo_ark_shrine_801836BC[18] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { -7000, 0x5A28, -1050 } }, 329 },
    { { { { 642, 0, -4045 }, { -628, 4046, -99 }, { 3996, 636, 634 } }, { -7100, 1660, -950 } }, 329 },
    { { { { 617, 0, 4049 }, { 342, 4081, -52 }, { -4034, 346, 615 } }, { -0x32C8, 1390, -860 } }, 275 },
    { { { { 4049, 0, -613 }, { -103, 4037, -681 }, { 604, 689, 3991 } }, { -6330, 1660, 4750 } }, 257 },
    { { { { 4082, 0, -331 }, { -43, 4061, -529 }, { 329, 531, 4047 } }, { -6720, 1475, 5 } }, 329 },
    { { { { -574, 0, -4055 }, { -1, 4096, 0 }, { 4055, 1, -574 } }, { 1295, 970, -5540 } }, 329 },
    { { { { -802, 0, 4016 }, { 90, 4094, 18 }, { -4015, 92, -801 } }, { -6180, 1120, -5590 } }, 329 },
    { { { { 4022, 0, -772 }, { -101, 4060, -528 }, { 765, 537, 3987 } }, { -365, 1325, 425 } }, 282 },
    { { { { -4086, 0, -277 }, { -137, 3557, 2026 }, { 240, 2030, -3548 } }, { -610, 2540, -4060 } }, 282 },
    { { { { -4063, 0, -515 }, { -123, 3977, 970 }, { 500, 978, -3945 } }, { -6730, 1660, -480 } }, 225 },
    { { { { -4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, -4096 } }, { -7000, 1554, 3630 } }, 541 },
    { { { { 4085, 0, -297 }, { -133, 3662, -1828 }, { 265, 1832, 3653 } }, { -6700, 2300, -1490 } }, 348 },
    { { { { -3829, 0, -1452 }, { 701, 3587, -1848 }, { 1272, -1977, -3353 } }, { -7890, 1790, 2780 } }, 312 },
    { { { { 3986, 0, -938 }, { 71, 4084, 303 }, { 936, -312, 3975 } }, { -6140, 980, 4450 } }, 269 },
    { { { { 407, 0, -4075 }, { -498, 4065, -49 }, { 4045, 500, 404 } }, { -900, 1100, -770 } }, 380 },
    { { { { 4049, 0, -613 }, { -103, 4037, -681 }, { 604, 689, 3991 } }, { -6330, 1660, 4750 } }, 257 },
    { { { { -4063, 0, -515 }, { -123, 3977, 970 }, { 500, 978, -3945 } }, { -6730, 1660, -480 } }, 225 },
    { { { { 4082, 0, -331 }, { -43, 4061, -529 }, { 329, 531, 4047 } }, { -6720, 1475, 5 } }, 329 },
};

SpriteBatch D_neo_ark_shrine_80183944[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_neo_ark_shrine_80183954[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_neo_ark_shrine_80183964[57] = {
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -72, -72, 1175, { .fields = { 80, 240 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 48 } }, -136, -120, 750, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 48 } }, -136, -72, 750, { .fields = { 104, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 48 } }, -136, -24, 750, { .fields = { 24, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 48 } }, -136, 24, 750, { .fields = { 24, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 48 } }, -136, 72, 750, { .fields = { 32, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 48 } }, -96, -24, 1062, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 48 } }, -96, -72, 1062, { .fields = { 8, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 48 } }, -96, -120, 1062, { .fields = { 16, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -72, -120, 1153, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 48 } }, -96, 24, 1062, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 48 } }, -96, 72, 1106, { .fields = { 40, 144 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 32 } }, -72, 40, 1153, { .fields = { 40, 88 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 40 } }, -72, 72, 1153, { .fields = { 40, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 32, -120, 1187, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 48 } }, 112, 72, 1150, { .fields = { 104, 48 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 48 } }, 64, 72, 1162, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 48 } }, 112, 24, 1150, { .fields = { 56, 48 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 48 } }, 112, -24, 1150, { .fields = { 56, 144 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 48 } }, 112, -72, 1150, { .fields = { 40, 192 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 48 } }, 112, -120, 1150, { .fields = { 88, 192 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 48 } }, 64, -120, 1162, { .fields = { 64, 96 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 48 } }, 64, -72, 1162, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 64, -24, 1162, { .fields = { 32, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 64, 24, 1162, { .fields = { 32, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 40, 24, 1187, { .fields = { 56, 144 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 24, 24 } }, 40, 72, 1187, { .fields = { 32, 152 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 8, 32 } }, 56, -8, 1187, { .fields = { 48, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 40, -120, 1187, { .fields = { 32, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 48, -72, 1187, { .fields = { 80, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 72, -56, 1950, { .fields = { 96, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 72, 24, 1950, { .fields = { 72, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -24, -8, 1950, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 24, -8, 1950, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -24, -56, 1950, { .fields = { 80, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 16 } }, 24, -120, 1950, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 24, -104, 1950, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, 24, -56, 1950, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 48 } }, 72, -104, 1950, { .fields = { 80, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 72, -120, 1950, { .fields = { 48, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 104, -120, 1950, { .fields = { 24, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, -72, -56, 1950, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, -72, -8, 1950, { .fields = { 64, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -88, -56, 1950, { .fields = { 64, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -88, -8, 1950, { .fields = { 64, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, -160, 72, 462, { .fields = { 112, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, -160, 24, 462, { .fields = { 112, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 48 } }, -160, -24, 462, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 48 } }, -160, -72, 462, { .fields = { 120, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 48 } }, -160, -120, 462, { .fields = { 120, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -88, 48, 812, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, -136, 56, 812, { .fields = { 72, 192 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 8, 40 } }, -88, 56, 812, { .fields = { 56, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -136, 8, 812, { .fields = { 80, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -136, -40, 812, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -136, -88, 812, { .fields = { 32, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 16 } }, -136, -104, 812, { .fields = { 80, 240 } }, 128, 128, 128, 0 },
};

SpriteBatch D_neo_ark_shrine_80183DD8[7] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 14, 0, 0, { 0, 0 } },
    { 14, 16, 0, 0, { 3, 0 } },
    { 30, 15, 0, 0, { 2, 0 } },
    { 45, 5, 0, 0, { 4, 0 } },
    { 50, 7, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_neo_ark_shrine_80183E10[36] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 24, -120, 1200, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 40, -72, 1200, { .fields = { 112, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 112, -120, 1087, { .fields = { 40, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 112, -72, 1087, { .fields = { 40, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 112, -24, 1087, { .fields = { 40, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 112, 24, 1087, { .fields = { 80, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, 72, -120, 1125, { .fields = { 88, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, 72, -72, 1125, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, 72, -24, 1125, { .fields = { 40, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, 72, 24, 1125, { .fields = { 88, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 48, -120, 1187, { .fields = { 88, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 48, -72, 1187, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 48, -24, 1187, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, 48, 24, 1187, { .fields = { 64, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 40, 24, 1200, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 32, -120, 1200, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -72, 72, 1062, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, -160, -120, 987, { .fields = { 120, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, -160, -72, 987, { .fields = { 120, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, -160, -24, 987, { .fields = { 72, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, -160, 24, 987, { .fields = { 120, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 24 } }, -160, 72, 987, { .fields = { 24, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 48 } }, -112, -120, 1000, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 48 } }, -112, -72, 1000, { .fields = { 112, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 48 } }, -112, -24, 1000, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 48 } }, -112, 24, 1000, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, -112, 72, 1000, { .fields = { 48, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -72, -120, 1075, { .fields = { 96, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -72, -72, 1062, { .fields = { 24, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -72, -24, 1062, { .fields = { 112, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -72, 24, 1062, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, -56, 0, 1075, { .fields = { 64, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 48 } }, -56, 24, 1075, { .fields = { 88, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 48 } }, -80, -32, 2125, { .fields = { 56, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 48 } }, -80, -80, 2125, { .fields = { 88, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, -72, -120, 2125, { .fields = { 56, 96 } }, 128, 128, 128, 0 },
};

SpriteBatch D_neo_ark_shrine_801840E0[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 16, 0, 0, { 1, 0 } },
    { 16, 17, 0, 0, { 2, 0 } },
    { 33, 3, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_neo_ark_shrine_80184108[9] = {
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -160, -120, 862, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -160, -72, 862, { .fields = { 80, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -160, -24, 862, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -160, 24, 862, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 32 } }, -160, 72, 862, { .fields = { 64, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -112, 72, 1000, { .fields = { 80, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -112, 24, 1000, { .fields = { 112, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -112, 0, 1000, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -112, -120, 1000, { .fields = { 56, 24 } }, 128, 128, 128, 0 },
};

SpriteBatch D_neo_ark_shrine_801841BC[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 9, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_neo_ark_shrine_801841D4[61] = {
    { 143, 0x3FC0, { .fields = { 56, 56 } }, 104, 24, 987, { .fields = { 72, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 56 } }, 104, -32, 987, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 56 } }, 104, -88, 987, { .fields = { 72, 112 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 56, 32 } }, 104, -120, 987, { .fields = { 8, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 56 } }, 64, 24, 987, { .fields = { 88, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 56 } }, 64, -32, 987, { .fields = { 48, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 56 } }, 64, -88, 987, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 32 } }, 64, -120, 987, { .fields = { 64, 224 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 32, 16 } }, 32, -120, 1025, { .fields = { 8, 192 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 32, 16 } }, 0, -120, 1025, { .fields = { 8, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, -160, 40, 950, { .fields = { 72, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 48 } }, -112, 40, 950, { .fields = { 40, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, -160, -8, 950, { .fields = { 80, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, -160, -56, 950, { .fields = { 80, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -160, -104, 950, { .fields = { 0, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 16 } }, -160, -120, 950, { .fields = { 32, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, -112, -8, 950, { .fields = { 32, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 48 } }, -112, -56, 950, { .fields = { 40, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 48 } }, -112, -104, 950, { .fields = { 72, 96 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 16 } }, -112, -120, 950, { .fields = { 0, 208 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 16 } }, -72, -120, 950, { .fields = { 0, 48 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 16 } }, -32, -120, 950, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 16, -56, 1950, { .fields = { 48, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 16, 40, 1950, { .fields = { 56, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, 24, 40, 1950, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, 24, -8, 1950, { .fields = { 24, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, 24, -56, 1950, { .fields = { 24, 48 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 48 } }, 24, -104, 1950, { .fields = { 56, 96 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 16 } }, 24, -120, 1950, { .fields = { 72, 240 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 32, 16 } }, -8, -120, 1950, { .fields = { 8, 64 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 32, 48 } }, -8, -104, 1950, { .fields = { 72, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -24, -120, 1950, { .fields = { 48, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -24, -104, 1950, { .fields = { 40, 248 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 48 } }, -40, -64, 3125, { .fields = { 120, 144 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 48 } }, -40, -16, 3125, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 48 } }, 8, -16, 3125, { .fields = { 104, 48 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 48 } }, 8, -64, 3125, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 48 } }, 40, 8, 1750, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, 48, -40, 1750, { .fields = { 40, 56 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 32, 32 } }, 48, -72, 1750, { .fields = { 40, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -112, -112, 600, { .fields = { 48, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, -160, -104, 600, { .fields = { 120, 56 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 16 } }, -160, -120, 600, { .fields = { 8, 128 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 8, 40 } }, -112, -104, 600, { .fields = { 64, 144 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 48 } }, -144, 40, 550, { .fields = { 104, 96 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 48 } }, -144, -8, 550, { .fields = { 72, 192 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 48 } }, -144, -56, 550, { .fields = { 72, 144 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 24, 16 } }, -128, -72, 550, { .fields = { 16, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -96, -24, 550, { .fields = { 64, 152 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 48 } }, -96, 40, 550, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 32, 16 } }, -96, 24, 550, { .fields = { 24, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -96, -8, 550, { .fields = { 56, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -48, 48, 550, { .fields = { 104, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -136, 72, 625, { .fields = { 0, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, -136, 24, 625, { .fields = { 112, 104 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 48 } }, -136, -24, 625, { .fields = { 120, 192 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 24 } }, -136, -48, 625, { .fields = { 8, 88 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 32, 32 } }, -88, -8, 625, { .fields = { 40, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 48 } }, -88, 24, 625, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 32, 40 } }, -88, 72, 625, { .fields = { 40, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -56, 40, 625, { .fields = { 24, 248 } }, 128, 128, 128, 0 },
};

SpriteBatch D_neo_ark_shrine_80184698[10] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 10, 0, 0, { 6, 0 } },
    { 10, 12, 0, 0, { 1, 0 } },
    { 22, 11, 0, 0, { 4, 0 } },
    { 33, 4, 0, 0, { 0, 0 } },
    { 37, 3, 0, 0, { 5, 0 } },
    { 40, 4, 0, 0, { 3, 0 } },
    { 44, 9, 0, 0, { 7, 0 } },
    { 53, 8, 0, 0, { 2, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_neo_ark_shrine_801846E8[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_neo_ark_shrine_801846F8[16] = {
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 112, 24, 925, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 112, -24, 925, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 112, -72, 925, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 112, -120, 925, { .fields = { 80, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 64, 24, 925, { .fields = { 80, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 64, -24, 925, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 64, -72, 925, { .fields = { 32, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 64, -120, 925, { .fields = { 32, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, 32, 24, 1125, { .fields = { 48, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 16, 24, 1137, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, 32, -24, 1125, { .fields = { 48, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, 32, -72, 1125, { .fields = { 16, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, 32, -120, 1125, { .fields = { 16, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 24, -120, 1125, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 24, -72, 1125, { .fields = { 24, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 16, -24, 1125, { .fields = { 16, 96 } }, 128, 128, 128, 0 },
};

SpriteBatch D_neo_ark_shrine_80184838[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 16, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_neo_ark_shrine_80184850[18] = {
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -160, -120, 937, { .fields = { 48, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -160, -72, 937, { .fields = { 48, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -160, -24, 937, { .fields = { 64, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -160, 24, 937, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -160, 72, 937, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, -112, -120, 1125, { .fields = { 32, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, -112, -72, 1125, { .fields = { 48, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, -112, -24, 1125, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, -112, 24, 1125, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -112, 72, 1125, { .fields = { 32, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -80, -120, 1275, { .fields = { 112, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -80, -72, 1275, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -80, -24, 1275, { .fields = { 112, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -80, 24, 1275, { .fields = { 96, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -64, -96, 1425, { .fields = { 40, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -64, -72, 1425, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -64, -24, 1425, { .fields = { 112, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -64, 24, 1425, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
};

SpriteBatch D_neo_ark_shrine_801849B8[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 18, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_neo_ark_shrine_801849D0[14] = {
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -160, 72, 450, { .fields = { 80, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -112, 104, 450, { .fields = { 80, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -160, 32, 450, { .fields = { 88, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -160, 0, 450, { .fields = { 56, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -160, -40, 450, { .fields = { 88, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -160, -80, 450, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -160, -104, 450, { .fields = { 80, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 112, 72, 450, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 120, 32, 450, { .fields = { 88, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 128, 0, 450, { .fields = { 72, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 136, -32, 450, { .fields = { 64, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 144, -72, 450, { .fields = { 112, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 152, -104, 450, { .fields = { 104, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 104, 104, 450, { .fields = { 80, 224 } }, 128, 128, 128, 0 },
};

SpriteBatch D_neo_ark_shrine_80184AE8[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 7, 0, 0, { 1, 0 } },
    { 7, 7, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_neo_ark_shrine_80184B08[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_neo_ark_shrine_80184B18[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_neo_ark_shrine_80184B28[28] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -112, -48, 575, { .fields = { 48, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 32 } }, -160, -120, 475, { .fields = { 80, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 32 } }, -112, -120, 475, { .fields = { 80, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 32 } }, -64, -120, 475, { .fields = { 56, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 32 } }, -16, -120, 475, { .fields = { 80, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 32 } }, 32, -120, 475, { .fields = { 80, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 24 } }, 80, -120, 475, { .fields = { 40, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, 120, -120, 475, { .fields = { 32, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 24 } }, -160, -88, 525, { .fields = { 32, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 24 } }, -112, -88, 525, { .fields = { 32, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 24 } }, -64, -88, 525, { .fields = { 32, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 24 } }, -16, -88, 525, { .fields = { 32, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 32, -88, 525, { .fields = { 48, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 16 } }, -160, -64, 575, { .fields = { 32, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 16 } }, -112, -64, 575, { .fields = { 16, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -64, -64, 575, { .fields = { 56, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 16 } }, -160, -48, 575, { .fields = { 8, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 32 } }, 88, -120, 475, { .fields = { 80, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 56, -120, 475, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 32 } }, 88, -88, 475, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 64, -88, 475, { .fields = { 104, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 24 } }, 88, -56, 525, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 72, -56, 525, { .fields = { 64, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 16 } }, 88, -32, 550, { .fields = { 32, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 80, -32, 550, { .fields = { 72, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 16 } }, 88, -16, 562, { .fields = { 8, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 80, -16, 562, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 88, 0, 575, { .fields = { 80, 24 } }, 128, 128, 128, 0 },
};

SpriteBatch D_neo_ark_shrine_80184D58[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 17, 0, 0, { 1, 0 } },
    { 17, 11, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_neo_ark_shrine_80184D78[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_neo_ark_shrine_80184D88[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_neo_ark_shrine_80184D98[34] = {
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -160, 32, 962, { .fields = { 24, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -160, -16, 962, { .fields = { 72, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -112, 32, 962, { .fields = { 80, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -112, -16, 962, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -160, -64, 962, { .fields = { 32, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -160, -112, 962, { .fields = { 32, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -160, -120, 962, { .fields = { 64, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -112, -64, 962, { .fields = { 72, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -112, -112, 962, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -112, -120, 962, { .fields = { 64, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -64, 32, 962, { .fields = { 72, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -64, -16, 962, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -64, -64, 962, { .fields = { 120, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, -64, -88, 962, { .fields = { 80, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 112, 16, 1000, { .fields = { 24, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 112, -32, 1000, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 48 } }, 112, -80, 1000, { .fields = { 104, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 40 } }, 112, -120, 1000, { .fields = { 64, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 48 } }, 72, 16, 1000, { .fields = { 120, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 48 } }, 72, -32, 1000, { .fields = { 112, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 48 } }, 72, -80, 1000, { .fields = { 120, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, 72, -120, 1000, { .fields = { 64, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 56, 16, 1112, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 40, 16, 1250, { .fields = { 88, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, 56, -32, 1112, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 48, -32, 1250, { .fields = { 120, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, 56, -80, 1112, { .fields = { 104, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 40, -80, 1250, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 56, -120, 1125, { .fields = { 56, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 40, -120, 1250, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, 24, -120, 1250, { .fields = { 88, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 48 } }, -80, -32, 1950, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 48 } }, -80, -80, 1950, { .fields = { 88, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, -72, -120, 1950, { .fields = { 64, 88 } }, 128, 128, 128, 0 },
};

SpriteBatch D_neo_ark_shrine_80185040[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 14, 0, 0, { 1, 0 } },
    { 14, 17, 0, 0, { 2, 0 } },
    { 31, 3, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_neo_ark_shrine_80185068[14] = {
    { 141, 0x3FC0, { .fields = { 48, 48 } }, -160, 72, 475, { .fields = { 0, 176 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 40 } }, -160, 32, 475, { .fields = { 0, 136 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 32, 32 } }, -160, 0, 475, { .fields = { 0, 104 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 24, 32 } }, -160, -32, 475, { .fields = { 0, 72 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 40 } }, -160, -72, 475, { .fields = { 0, 32 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 8, 32 } }, -160, -104, 475, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 8, 16 } }, -112, 104, 485, { .fields = { 48, 208 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 48 } }, 112, 72, 475, { .fields = { 8, 176 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 40 } }, 120, 32, 475, { .fields = { 16, 136 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 32, 32 } }, 128, 0, 475, { .fields = { 24, 104 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 24, 32 } }, 136, -32, 475, { .fields = { 32, 72 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 8, 32 } }, 152, -104, 475, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 40 } }, 144, -72, 475, { .fields = { 40, 32 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 8, 16 } }, 104, 104, 475, { .fields = { 0, 208 } }, 128, 128, 128, 0 },
};

SpriteBatch D_neo_ark_shrine_80185180[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 7, 0, 0, { 1, 0 } },
    { 7, 7, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_neo_ark_shrine_801851A0[10] = {
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -160, 72, 875, { .fields = { 80, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -112, 72, 875, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -88, 72, 1000, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -160, 24, 875, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -112, 24, 875, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -112, 0, 875, { .fields = { 72, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -160, -24, 875, { .fields = { 80, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -160, -72, 875, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -160, -120, 875, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -112, -120, 875, { .fields = { 56, 72 } }, 128, 128, 128, 0 },
};

SpriteBatch D_neo_ark_shrine_80185268[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 10, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

GpSprtRec D_neo_ark_shrine_80185280[18] = {
    { { .empty = D_neo_ark_shrine_80183944 }, D_neo_ark_shrine_80183944, NULL },
    { { .empty = D_neo_ark_shrine_80183954 }, D_neo_ark_shrine_80183954, NULL },
    { { .elements = D_neo_ark_shrine_80183964 }, D_neo_ark_shrine_80183DD8, NULL },
    { { .elements = D_neo_ark_shrine_80183E10 }, D_neo_ark_shrine_801840E0, NULL },
    { { .elements = D_neo_ark_shrine_80184108 }, D_neo_ark_shrine_801841BC, NULL },
    { { .elements = D_neo_ark_shrine_801841D4 }, D_neo_ark_shrine_80184698, NULL },
    { { .empty = D_neo_ark_shrine_801846E8 }, D_neo_ark_shrine_801846E8, NULL },
    { { .elements = D_neo_ark_shrine_801846F8 }, D_neo_ark_shrine_80184838, NULL },
    { { .elements = D_neo_ark_shrine_80184850 }, D_neo_ark_shrine_801849B8, NULL },
    { { .elements = D_neo_ark_shrine_801849D0 }, D_neo_ark_shrine_80184AE8, NULL },
    { { .empty = D_neo_ark_shrine_80184B08 }, D_neo_ark_shrine_80184B08, NULL },
    { { .empty = D_neo_ark_shrine_80184B18 }, D_neo_ark_shrine_80184B18, NULL },
    { { .elements = D_neo_ark_shrine_80184B28 }, D_neo_ark_shrine_80184D58, NULL },
    { { .empty = D_neo_ark_shrine_80184D78 }, D_neo_ark_shrine_80184D78, NULL },
    { { .empty = D_neo_ark_shrine_80184D88 }, D_neo_ark_shrine_80184D88, NULL },
    { { .elements = D_neo_ark_shrine_80184D98 }, D_neo_ark_shrine_80185040, NULL },
    { { .elements = D_neo_ark_shrine_80185068 }, D_neo_ark_shrine_80185180, NULL },
    { { .elements = D_neo_ark_shrine_801851A0 }, D_neo_ark_shrine_80185268, NULL },
};

GpLight D_neo_ark_shrine_80185358[2] = {
    { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -332, -462, -388 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 750, 745, 740, { 0, 0 } },
    { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 267, -272, 272 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 348, 286, 245, { 0, 0 } },
};

GpPointLight D_neo_ark_shrine_80185408[17] = {
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 1198, -2188, 3758 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 4100, 4038, 3936, { 0, 0 } }, 832, 4161 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 6232, -1290, -505 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 1845, 1765, 1703, { 0, 0 } }, 1685, 3529 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 6055, -1410, 3087 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 1475, 1311, 1232, { 0, 0 } }, 992, 2101 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 3560, -1430, 3820 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 1843, 1515, 1187, { 0, 0 } }, 600, 1400 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 4910, -1430, 3820 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2457, 2170, 1843, { 0, 0 } }, 900, 1180 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 4579, -1430, 5537 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 1621, 1496, 1375, { 0, 0 } }, 900, 1902 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 3560, -1430, 6155 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 1293, 1272, 1232, { 0, 0 } }, 363, 941 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 7688, -1235, 4382 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 1478, 1434, 1372, { 0, 0 } }, 1201, 1931 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x2CEB, -1360, 713 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2460, 2170, 1844, { 0, 0 } }, 881, 1663 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x2E28, -1320, 2500 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2461, 2174, 1847, { 0, 0 } }, 1159, 1985 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x27BB, -1360, 2967 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 0, 0, 0, { 0, 0 } }, 5, 357 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 9402, -1360, 2037 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2458, 2171, 1844, { 0, 0 } }, 1182, 2084 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x2869, -1360, 285 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 309, 226, 184, { 0, 0 } }, 150, 900 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 9060, -1360, 445 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2457, 2170, 1843, { 0, 0 } }, 900, 1603 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 7005, -2049, -3922 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2376, 2338, 2276, { 0, 0 } }, 850, 3850 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -10, -2789, 1661 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 4567, 4550, 4508, { 0, 0 } }, 2520, 6612 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -25, -2065, 1050 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 1802, 1863, 1884, { 0, 0 } }, 200, 2200 },
};

GpRoomCoordSet D_neo_ark_shrine_80185A68[1] = {
    { 2, D_neo_ark_shrine_80185358, 17, D_neo_ark_shrine_80185408, 0, NULL },
};

GpObj4C D_neo_ark_shrine_80185A80[14] = {
    { NULL, NULL, NULL, { 7039, -2432, -2210, 0 }, { { 2687, -2832, -10, 0 }, { -2698, -2832, 1, 0 }, { 2687, 2832, -10, 0 }, { -2698, 2832, 1, 0 } }, { 8, 0, 4099, 0 }, { 0, 0, 4096, 0 }, 3907, 0, 4, 10, 1, 0 },
    { NULL, NULL, NULL, { 7087, -2401, -2096, 0 }, { { -2713, -2800, 14, 0 }, { 2705, -2800, -23, 0 }, { -2713, 2800, 14, 0 }, { 2705, 2800, -23, 0 } }, { -29, 0, -4099, 0 }, { 0, 0, 4096, 0 }, 3890, 0, 10, 4, 1, 0 },
    { NULL, NULL, NULL, { 8224, -2369, 1312, 0 }, { { 0, -2832, 1984, 0 }, { 0, -2832, -1984, 0 }, { 0, 2832, 1984, 0 }, { 0, 2832, -1984, 0 } }, { -4112, 0, 0, 0 }, { 0, 0, 4096, 0 }, 3453, 0, 4, 3, 1, 0 },
    { NULL, NULL, NULL, { 8079, -2481, 1391, 0 }, { { 4, -2784, -1833, 0 }, { -3, -2784, 1834, 0 }, { 4, 2784, -1833, 0 }, { -3, 2784, 1834, 0 } }, { 4111, 0, 7, 0 }, { 0, 0, 4096, 0 }, 3328, 0, 3, 4, 1, 0 },
    { NULL, NULL, NULL, { 0x28D1, -2416, 1408, 0 }, { { 208, -2880, -2032, 0 }, { -208, -2880, 2032, 0 }, { 208, 2880, -2032, 0 }, { -208, 2880, 2032, 0 } }, { 4088, 0, 418, 0 }, { 0, 0, 4096, 0 }, 3528, 0, 2, 3, 1, 0 },
    { NULL, NULL, NULL, { 0x2960, -2416, 1504, 0 }, { { -224, -2816, 2144, 0 }, { 224, -2816, -2144, 0 }, { -224, 2816, 2144, 0 }, { 224, 2816, -2144, 0 } }, { -4074, 0, -426, 0 }, { 0, 0, 4096, 0 }, 3537, 0, 3, 2, 1, 0 },
    { NULL, NULL, NULL, { 6767, -2545, 2671, 0 }, { { 1946, -2848, 68, 0 }, { -1945, -2848, -67, 0 }, { 1946, 2849, 68, 0 }, { -1945, 2849, -67, 0 } }, { -143, 0, 4108, 0 }, { 0, 0, 4096, 0 }, 3444, 0, 5, 4, 1, 0 },
    { NULL, NULL, NULL, { 6751, -2496, 2751, 0 }, { { -1810, -2800, -67, 0 }, { 1810, -2800, 68, 0 }, { -1810, 2800, -67, 0 }, { 1810, 2800, 68, 0 } }, { 152, 0, -4106, 0 }, { 0, 0, 4096, 0 }, 3328, 0, 4, 5, 1, 0 },
    { NULL, NULL, NULL, { 1535, -2496, 5119, 0 }, { { 408, -2864, -2184, 0 }, { -418, -2864, 2174, 0 }, { 408, 2864, -2184, 0 }, { -418, 2864, 2174, 0 } }, { 4033, 0, 764, 0 }, { 0, 0, 4096, 0 }, 3620, 0, 6, 8, 1, 0 },
    { NULL, NULL, NULL, { 1630, -2480, 5215, 0 }, { { -415, -2848, 2178, 0 }, { 411, -2848, -2181, 0 }, { -415, 2848, 2178, 0 }, { 411, 2848, -2181, 0 } }, { -4028, 0, -764, 0 }, { 0, 0, 4096, 0 }, 3602, 0, 8, 6, 1, 0 },
    { NULL, NULL, NULL, { 1054, -2400, 2080, 0 }, { { -2215, -2928, 128, 0 }, { 2215, -2928, -129, 0 }, { -2215, 2928, 128, 0 }, { 2215, 2928, -129, 0 } }, { -238, 0, -4097, 0 }, { 0, 0, 4096, 0 }, 3665, 0, 9, 8, 1, 0 },
    { NULL, NULL, NULL, { 1086, -2384, 1984, 0 }, { { 2215, -2848, -129, 0 }, { -2215, -2848, 128, 0 }, { 2215, 2848, -129, 0 }, { -2215, 2848, 128, 0 } }, { 237, 0, 4092, 0 }, { 0, 0, 4096, 0 }, 3602, 0, 8, 9, 1, 0 },
    { NULL, NULL, NULL, { 5631, -2368, 5472, 0 }, { { -71, -2848, 1942, 0 }, { 64, -2848, -1948, 0 }, { -71, 2849, 1942, 0 }, { 64, 2849, -1948, 0 } }, { -4109, 0, -143, 0 }, { 0, 0, 4096, 0 }, 3444, 0, 6, 5, 1, 0 },
    { NULL, NULL, NULL, { 5535, -2464, 5472, 0 }, { { 65, -2848, -1947, 0 }, { -70, -2848, 1943, 0 }, { 65, 2849, -1947, 0 }, { -70, 2849, 1943, 0 } }, { 4107, 0, 141, 0 }, { 0, 0, 4096, 0 }, 3444, 0, 5, 6, 129, 0 },
};

GpObj4C D_neo_ark_shrine_80185EA8[9] = {
    { NULL, NULL, NULL, { 400, -48, 1088, 0 }, { { -496, 0, -768, 0 }, { 496, 0, -768, 0 }, { -496, 0, 768, 0 }, { 496, 0, 768, 0 } }, { 0, 4097, 0, 0 }, { 4096, 0, 0, 0 }, 914, 0, 17, 17, 2, 0 },
    { NULL, NULL, NULL, { 0x3100, -48, 1504, 0 }, { { -496, 0, -1024, 0 }, { 496, 0, -1024, 0 }, { -496, 0, 1024, 0 }, { 496, 0, 1024, 0 } }, { 0, 4097, 0, 0 }, { -4096, 0, 0, 0 }, 1137, 0, 18, 33, 2, 0 },
    { NULL, NULL, NULL, { 7008, -64, -4224, 0 }, { { -400, 0, -320, 0 }, { 400, 0, -320, 0 }, { -400, 0, 320, 0 }, { 400, 0, 320, 0 } }, { 0, 4095, 0, 0 }, { 151, 0, 4093, 0 }, 512, 0x4005, 1, 0, 2, 0 },
    { NULL, NULL, NULL, { 1728, -64, 976, 0 }, { { -496, 0, -656, 0 }, { 496, 0, -656, 0 }, { -496, 0, 656, 0 }, { 496, 0, 656, 0 } }, { 0, 4098, 0, 0 }, { -4096, 0, 0, 0 }, 822, 2, 5, 255, 2, 0 },
    { NULL, NULL, NULL, { 6992, -64, 5616, 0 }, { { -1024, 0, -464, 0 }, { 1024, 0, -464, 0 }, { -1024, 0, 464, 0 }, { 1024, 0, 464, 0 } }, { 0, 4098, 0, 0 }, { 0, 0, -4096, 0 }, 1123, 2, 8, 0, 2, 0 },
    { NULL, NULL, NULL, { 5216, -64, -3840, 0 }, { { -496, 0, -1184, 0 }, { 496, 0, -1184, 0 }, { -496, 0, 1184, 0 }, { 496, 0, 1184, 0 } }, { 0, 4099, 0, 0 }, { 4096, 0, 0, 0 }, 1280, 2, 10, 0, 2, 0 },
    { NULL, NULL, NULL, { 8864, -64, -3840, 0 }, { { -496, 0, -1184, 0 }, { 496, 0, -1184, 0 }, { -496, 0, 1184, 0 }, { 496, 0, 1184, 0 } }, { 0, 4099, 0, 0 }, { -4096, 0, 0, 0 }, 1280, 2, 10, 0, 2, 0 },
    { NULL, NULL, NULL, { 5599, -64, -2273, 0 }, { { -954, 0, -858, 0 }, { -57, 0, -1282, 0 }, { 58, 0, 1283, 0 }, { 955, 0, 859, 0 } }, { 0, 4099, 0, 0 }, { 3513, 0, -2106, 0 }, 1280, 2, 11, 0, 2, 0 },
    { NULL, NULL, NULL, { 8479, -64, -2048, 0 }, { { 368, 0, -1230, 0 }, { 1135, 0, -600, 0 }, { -1135, 0, 600, 0 }, { -368, 0, 1230, 0 } }, { 0, 4099, 0, 0 }, { -3290, 0, -2440, 0 }, 1280, 2, 11, 0, 130, 0 },
};

GpObj4C D_neo_ark_shrine_80186154[8] = {
    { NULL, NULL, NULL, { 400, -48, 1344, 0 }, { { -496, 0, -1024, 0 }, { 496, 0, -1024, 0 }, { -496, 0, 1024, 0 }, { 496, 0, 1024, 0 } }, { 0, 4097, 0, 0 }, { 4096, 0, 0, 0 }, 1137, 0, 17, 17, 2, 0 },
    { NULL, NULL, NULL, { 0x3100, -48, 1504, 0 }, { { -496, 0, -1024, 0 }, { 496, 0, -1024, 0 }, { -496, 0, 1024, 0 }, { 496, 0, 1024, 0 } }, { 0, 4097, 0, 0 }, { -4096, 0, 0, 0 }, 1137, 0, 18, 33, 2, 0 },
    { NULL, NULL, NULL, { 7024, -64, -4048, 0 }, { { -1152, 0, -560, 0 }, { 1152, 0, -560, 0 }, { -1152, 0, 560, 0 }, { 1152, 0, 560, 0 } }, { 0, 4114, 0, 0 }, { 601, 0, 4052, 0 }, 1280, 0x4005, 1, 0, 2, 0 },
    { NULL, NULL, NULL, { 7008, -64, 3680, 0 }, { { -1152, 0, -560, 0 }, { 1152, 0, -560, 0 }, { -1152, 0, 560, 0 }, { 1152, 0, 560, 0 } }, { 0, 4114, 0, 0 }, { -601, 0, -4052, 0 }, 1280, 2, 7, 255, 2, 0 },
    { NULL, NULL, NULL, { 5184, -64, -3696, 0 }, { { -496, 0, -1264, 0 }, { 496, 0, -1264, 0 }, { -496, 0, 1264, 0 }, { 496, 0, 1264, 0 } }, { 0, 4105, 0, 0 }, { 4096, 0, 0, 0 }, 1354, 2, 10, 0, 2, 0 },
    { NULL, NULL, NULL, { 8864, -64, -3744, 0 }, { { -496, 0, -1248, 0 }, { 496, 0, -1248, 0 }, { -496, 0, 1248, 0 }, { 496, 0, 1248, 0 } }, { 0, 4097, 0, 0 }, { -4096, 0, 0, 0 }, 1342, 2, 10, 0, 2, 0 },
    { NULL, NULL, NULL, { 8511, -64, -2153, 0 }, { { 249, 0, -1143, 0 }, { 1015, 0, -514, 0 }, { -1046, 0, 531, 0 }, { -216, 0, 1128, 0 } }, { 0, 4095, 0, 0 }, { -3035, 0, -2751, 0 }, 1166, 2, 11, 0, 2, 0 },
    { NULL, NULL, NULL, { 5599, -64, -2209, 0 }, { { -941, 0, -913, 0 }, { -25, 0, -1293, 0 }, { 26, 0, 1422, 0 }, { 942, 0, 786, 0 } }, { 0, 4099, 0, 0 }, { 3166, 0, -2598, 0 }, 1419, 2, 11, 0, 130, 0 },
};

GpObj4C D_neo_ark_shrine_801863B4[8] = {
    { NULL, NULL, NULL, { 400, -48, 1344, 0 }, { { -496, 0, -1024, 0 }, { 496, 0, -1024, 0 }, { -496, 0, 1024, 0 }, { 496, 0, 1024, 0 } }, { 0, 4097, 0, 0 }, { 4096, 0, 0, 0 }, 1137, 0, 17, 17, 2, 0 },
    { NULL, NULL, NULL, { 0x3100, -48, 1504, 0 }, { { -496, 0, -1024, 0 }, { 496, 0, -1024, 0 }, { -496, 0, 1024, 0 }, { 496, 0, 1024, 0 } }, { 0, 4097, 0, 0 }, { -4096, 0, 0, 0 }, 1137, 0, 18, 33, 2, 0 },
    { NULL, NULL, NULL, { 7024, -64, -4016, 0 }, { { -1152, 0, -560, 0 }, { 1152, 0, -560, 0 }, { -1152, 0, 560, 0 }, { 1152, 0, 560, 0 } }, { 0, 4114, 0, 0 }, { 601, 0, 4052, 0 }, 1280, 0x4005, 1, 0, 2, 0 },
    { NULL, NULL, NULL, { 7008, -64, -1248, 0 }, { { -1152, 0, -560, 0 }, { 1152, 0, -560, 0 }, { -1152, 0, 560, 0 }, { 1152, 0, 560, 0 } }, { 0, 4114, 0, 0 }, { 201, 0, -4092, 0 }, 1280, 2, 6, 0, 2, 0 },
    { NULL, NULL, NULL, { 5184, -64, -3856, 0 }, { { -496, 0, -1232, 0 }, { 496, 0, -1232, 0 }, { -496, 0, 1232, 0 }, { 496, 0, 1232, 0 } }, { 0, 4112, 0, 0 }, { 4096, 0, 0, 0 }, 1324, 2, 10, 0, 2, 0 },
    { NULL, NULL, NULL, { 8960, -64, -3776, 0 }, { { -496, 0, -1280, 0 }, { 496, 0, -1280, 0 }, { -496, 0, 1280, 0 }, { 496, 0, 1280, 0 } }, { 0, 4113, 0, 0 }, { -4096, 0, 0, 0 }, 1372, 2, 10, 0, 2, 0 },
    { NULL, NULL, NULL, { 5631, -64, -2321, 0 }, { { -911, 0, -789, 0 }, { -14, 0, -1213, 0 }, { 15, 0, 1214, 0 }, { 912, 0, 790, 0 } }, { 0, 4113, 0, 0 }, { 3784, 0, -1567, 0 }, 1207, 2, 11, 0, 2, 0 },
    { NULL, NULL, NULL, { 8399, -64, -2161, 0 }, { { 317, 0, -1041, 0 }, { 1052, 0, -375, 0 }, { -1051, 0, 376, 0 }, { -316, 0, 1042, 0 } }, { 0, 4098, 0, 0 }, { -3290, 0, -2440, 0 }, 1115, 2, 11, 0, 130, 0 },
};

GpAreaTmdRec D_neo_ark_shrine_80186614[2] = {
    { 38, 38, 0, 0, { 0, 0 }, D_80137D74 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_neo_ark_shrine_8018662C[2] = {
    { 25, 25, 0, 0, { 0, 0 }, D_801379A8 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_neo_ark_shrine_80186644[2] = {
    { 25, 25, 0, 0, { 0, 0 }, D_801379A8 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_neo_ark_shrine_8018665C[3] = {
    { 38, 38, 0, 0, { 0, 0 }, D_80137D74 },
    { 25, 25, 1, 0, { 0, 0 }, D_8014F9A8 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_neo_ark_shrine_80186680[2] = {
    { 20, 20, 0, 0, { 0, 0 }, D_80147DF0 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_neo_ark_shrine_80186698[2] = {
    { 22, 22, 3, 0, { 0, 0 }, D_80154188 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_neo_ark_shrine_801866B0[2] = {
    { 39, 39, 3, 0, { 0, 0 }, D_801540E0 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaVariant D_neo_ark_shrine_801866C8[13] = {
    { NULL, NULL },
    { D_map_neo_ark_8017BEB0, D_neo_ark_shrine_80186614 },
    { D_map_neo_ark_8017BEF0, D_neo_ark_shrine_8018662C },
    { D_map_neo_ark_8017BF40, D_neo_ark_shrine_80186644 },
    { D_map_neo_ark_8017BF90, D_neo_ark_shrine_8018665C },
    { D_map_neo_ark_8017C020, D_neo_ark_shrine_80186680 },
    { NULL, NULL },
    { D_map_neo_ark_8017C050, D_neo_ark_shrine_80186698 },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { D_map_neo_ark_8017C070, D_neo_ark_shrine_801866B0 },
    { NULL, NULL },
};

GpObj3A D_neo_ark_shrine_80186730[4] = {
    { NULL, NULL, { 2736, -2352, 2159, 0 }, { { -784, 3376, 1841, 0 }, { 784, 3376, -1840, 0 }, { -784, -3376, 1841, 0 }, { 784, -3376, -1840, 0 } }, { 3777, 0, 1608, 0 }, { 84, 15 }, 1, 0 },
    { NULL, NULL, { 3119, -2080, -289, 0 }, { { 2728, 3376, 4275, 0 }, { -2727, 3376, -4275, 0 }, { 2728, -3376, 4275, 0 }, { -2727, -3376, -4275, 0 } }, { 3469, 0, -2214, 0 }, { -65, 23 }, 1, 0 },
    { NULL, NULL, { 0x2800, -1888, -2288, 0 }, { { -1840, 3376, 2657, 0 }, { 1840, 3376, -2657, 0 }, { -1840, -3376, 2657, 0 }, { 1840, -3376, -2657, 0 } }, { 3371, 0, 2334, 0 }, { 56, 18 }, 1, 0 },
    { NULL, NULL, { 0x2840, -1984, 4256, 0 }, { { 1904, 3376, 1617, 0 }, { -1904, 3376, -1616, 0 }, { 1904, -3376, 1617, 0 }, { -1904, -3376, -1616, 0 } }, { 2664, 0, -3139, 0 }, { 94, 16 }, 129, 0 },
};

s32 D_neo_ark_shrine_80186820[3] = {
    0x1000003D,
    0x1000003F,
    0x1000003D,
};

GpRoomParamRec D_neo_ark_shrine_8018682C[1] = {
    { 0, 0, 1, 0, D_neo_ark_shrine_80186820 },
};

GpRoomParamRec D_neo_ark_shrine_80186834[1] = {
    { 0, 0, 1, 0, NULL },
};

GpRoomParamRec D_neo_ark_shrine_8018683C[1] = {
    { 0, 0, 1, 0, D_neo_ark_shrine_80186820 },
};

GpRoomParamRec* D_neo_ark_shrine_80186844[8] = {
    D_neo_ark_shrine_8018682C,
    D_neo_ark_shrine_80186834,
    D_neo_ark_shrine_8018683C,
    D_neo_ark_shrine_8018682C,
    D_neo_ark_shrine_8018682C,
    D_neo_ark_shrine_8018682C,
    D_neo_ark_shrine_8018682C,
    D_neo_ark_shrine_8018682C,
};

Task* D_neo_ark_shrine_80186864 = NULL;

s16 D_neo_ark_shrine_80186868 = 0;

s16 D_neo_ark_shrine_8018686C[16] = { 0 };

NeoArkShrineSlot D_neo_ark_shrine_8018688C[16] = { 0 };

NeoArkShrineSlot D_neo_ark_shrine_801868CC[16] = { 0 };

static void func_neo_ark_shrine_8017E528(Task* task);

/// Moves the action-prompt cursor from the pad: for each port the task's
/// `spawnArg1` selects, integrates the analog stick and the d-pad direction
/// into the cursor's fixed-point position, clamps it to the screen, updates
/// the two prompt buttons' press / hold states, and draws the cursor icon.
static void func_neo_ark_shrine_8017E528(Task* task)
{
    RoomActionPrompt* prompt;
    PadState*         pad;
    s32               port;
    s32               first;
    s32               count;
    s32               status;
    s32               stick;
    s32               step;
    s32               mask;
    s32               speed;
    s32               i;
    s32               idx;
    u16*              statep;
    u16*              heldp;

    switch (task->spawnArg1.value) {
        case 1:
            first = 0;
            count = 1;
            break;
        case 2:
            first = 1;
            count = 2;
            break;
        default:
            first = 0;
            count = 2;
            break;
    }

    for (port = first; port < count; port++) {
        prompt = &D_80114D28[port];
        pad    = &Pad_States[port];
        status = pad->status;
        if (status == 0x12) {
            speed            = prompt->targetId;
            step             = ((u16)pad->field_54 << 0x10) >> 0x15;
            prompt->field_0 += step * speed * gDisplayState.frameTicks;
            step             = ((u16)pad->field_56 << 0x10) >> 0x15;
            prompt->field_4 += step * speed * gDisplayState.frameTicks;
        } else if (status == 0x73) {
            stick = pad->field_54;
            step  = (stick * stick) >> 0x15;
            if (stick < 0) {
                step = -step;
            }
            prompt->field_0 += step * prompt->targetId * gDisplayState.frameTicks;
            stick            = pad->field_56;
            step             = (stick * stick) >> 0x15;
            if (stick < 0) {
                step = -step;
            }
            prompt->field_4 += step * prompt->targetId * gDisplayState.frameTicks;
        }

        switch (pad->buttons >> 0xC) {
            case 1:
                step = 0x0;
                break;
            case 3:
                step = 0x200;
                break;
            case 2:
                step = 0x400;
                break;
            case 6:
                step = 0x600;
                break;
            case 4:
                step = 0x800;
                break;
            case 12:
                step = 0xA00;
                break;
            case 8:
                step = 0xC00;
                break;
            case 9:
                step = 0xE00;
                break;
            default:
                step = -1;
                break;
        }

        if (step != -1) {
            prompt->field_4 += (-rcos(step) * prompt->targetId * gDisplayState.frameTicks) >> 9;
            prompt->field_0 += (rsin(step) * prompt->targetId * gDisplayState.frameTicks) >> 9;
        }

        if (prompt->field_0 < -0x14000) {
            prompt->field_0 = -0x14000;
        } else if (prompt->field_0 > 0x13E00) {
            prompt->field_0 = 0x13E00;
        }
        if (prompt->field_4 < -0xDC00) {
            prompt->field_4 = -0xDC00;
        } else if (prompt->field_4 > 0xDC00) {
            prompt->field_4 = 0xDC00;
        }

        statep = &prompt->buttons.halfwords[0];
        heldp  = &prompt->buttons.halfwords[1];
        idx    = 0;
        for (i = 0; i < 2; i++, statep += 4, idx += 4) {
            mask = (i == 0) ? 0x40 : 0xA0;
            if (Pad_CheckButtons(port, 1, mask) != 0) {
                if (heldp[idx] < prompt->field_E &&
                    PARENT_OF(heldp + idx, RoomActionPromptButton, heldFrames)->lastPos == prompt->screen.packed) {
                    *statep    = 4;
                    heldp[idx] = prompt->field_E;
                } else {
                    heldp[idx]                                                          = 0;
                    PARENT_OF(heldp + idx, RoomActionPromptButton, heldFrames)->lastPos = prompt->screen.packed;
                    *statep                                                             = 2;
                }
            } else if (Pad_CheckButtons(port, 3, mask) != 0) {
                *statep = 3;
            } else if (Pad_CheckButtons(port, 0, mask) != 0) {
                *statep = 1;
            } else {
                *statep = 0;
            }
            heldp[idx] += gDisplayState.frameTicks;
        }

        prompt->screen.xy.x = prompt->field_0 >> 9;
        prompt->screen.xy.y = prompt->field_4 >> 9;
        func_neo_ark_shrine_8017E988(prompt->screen.xy.x, prompt->screen.xy.y, prompt->mode);
    }
}

/// Queues the action-prompt cursor icon, a 16x24 textured quad, at (`x`, `y`)
/// into the head of the current OT. `variant` selects the palette, 0x3C87 when
/// it is 2 and 0x3C88 otherwise, and 0 draws nothing.
static void func_neo_ark_shrine_8017E988(s32 x, s32 y, s32 variant)
{
    POLY_FT4* prim;
    s16       px;
    s16       py;

    if (variant == 0) {
        return;
    }

    prim           = gGpuPrimCursor;
    gGpuPrimCursor = prim + 1;

    px       = x - 2;
    prim->x2 = px;
    prim->x0 = px;
    px       = x + 0xE;
    prim->x3 = px;
    prim->x1 = px;
    py       = y - 2;
    prim->y1 = py;
    prim->y0 = py;
    py       = y + 0x15;
    prim->y3 = py;
    prim->y2 = py;

    prim->tpage = 0x1E;
    if (variant == 2) {
        prim->clut = 0x3C87;
    } else {
        prim->clut = 0x3C88;
    }

    setUVWH(prim, 0, 0xE8, 0x10, 0x17);
    setlen(prim, 9);
    setcode(prim, 0x2D);

    addPrim(gGpuCurrentOt, prim);
}

/// Task callback of the action-prompt cursor: state 0 resets both prompt slots,
/// state 1 moves the cursor from the pad every frame after.
void func_neo_ark_shrine_8017EA70(Task* task)
{
    TaskFunc states[2] = { func_neo_ark_shrine_8017F80C, func_neo_ark_shrine_8017E528 };

    states[task->state](task);
}

/// Per-frame helper of the cap script: animates and draws the sliding-tile
/// puzzle.
void func_neo_ark_shrine_8017EAC0()
{
    func_neo_ark_shrine_8017DF7C();
}

/// Task callback of the shrine's cap script: dispatches `Task::state` through a
/// copy of the script's state table.
void func_neo_ark_shrine_8017EAE0(Task* task)
{
    TaskFuncTable16 sp;

    sp = D_neo_ark_shrine_8017D5D0;
    sp.funcs[task->state](task);
}

/// Task callback of the shrine's first falling prop: dispatches `Task::state`
/// through a copy of the prop's state table.
void func_neo_ark_shrine_8017EB54(Task* task)
{
    TaskFuncTable4 states;

    states = D_neo_ark_shrine_8017D610;
    states.funcs[task->state](task);
}

/// Task callback of the shrine's second falling prop: dispatches `Task::state`
/// through a copy of the prop's state table.
void func_neo_ark_shrine_8017EBB8(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_neo_ark_shrine_8017D620;
    sp.funcs[task->state](task);
}

/// Hit-tests (`x`, `y`) against every rectangle of the `-1`-terminated hotspot
/// table, setting each entry's `hit` flag, and returns whether any was hit.
s32 func_neo_ark_shrine_8017EC10(OverlayHotspot* table, s16 x, s16 y)
{
    s32 hit;

    hit = 0;
    while (table->id != -1) {
        if ((x >= table->x) && ((table->x + table->w) >= x) && (y >= table->y) && ((table->y + table->h) >= y)) {
            table->hit = 1;
            hit        = 1;
        } else {
            table->hit = 0;
        }
        table++;
    }
    return hit;
}

/// Task callback of the descriptor at `D_neo_ark_shrine_80182404`: allocates
/// the cap script's state, sets the global mode byte, steps the task on one
/// state and clears the shrine's hotspot list.
static void func_neo_ark_shrine_8017ECC4(Task* task)
{
    NeoArkShrineScript* st;
    OverlayHotspot*     hs;

    st = memCalloc(0x10, 0);
    if (st == NULL) {
        taskKill(task);
        return;
    }
    task->spawnArg2.pointer           = Task_SpawnFromTable(D_neo_ark_shrine_80182404, 0, 1, 0);
    task->work                        = st;
    Mc_SaveData[0].state.at4.loc.view = 0xB;
    /* The once-loop folds away, but flow counts its references at loop depth
       2: without it the parameter's priority (6*2/42) loses to the state
       pointer's (3*1/10) and the two swap callee-saved homes. Keeping the
       state load below the mode store is the same loop's scheduling edge. */
    do {
        task->state++;
    } while (0);
    Display_AcquireRef();
    for (hs = D_neo_ark_shrine_80182430; hs->id != -1; hs++) {
        hs->hit = 0;
    }
    gGameSession->cutsceneHold = 1;
    gGameSession->hideHud      = 1;
    gGameSession->eventState   = 1;
}

/// Cap script state 1: sets the first action prompt's `targetId` to 0x80 and its
/// `mode` to 1, zeroes its on-screen position, and steps the script on.
static void func_neo_ark_shrine_8017EDAC(Task* task)
{
    RoomActionPrompt* prompt = D_80114D28;

    prompt->targetId    = 0x80;
    prompt->mode        = 1;
    prompt->screen.xy.x = 0;
    prompt->screen.xy.y = 0;
    task->state         = task->state + 1;
}

/// Spawns the action prompt for the script's current step: runs the shrine's
/// per-step helper, clears the prompt's highlight state, then re-spawns the
/// prompt at the coordinates the gameplay side left in `D_80114D28` with the
/// display mode this step picked, and advances the task to state 4.
static void func_neo_ark_shrine_8017EDE0(Task* task)
{
    RoomActionPrompt*   prompt = D_80114D28;
    NeoArkShrineScript* work   = (NeoArkShrineScript*)task->work;

    func_neo_ark_shrine_8017EAC0(task);
    prompt->mode     = 0;
    prompt->targetId = 0;
    func_800D4E78(prompt->screen.xy.x, prompt->screen.xy.y, work->field_E);
    task->state = 4;
}

/// Clears the action prompt's highlight state and runs the shrine's per-step
/// helper. When `func_800D4EC0` reports success, starts cap slot 2 if the
/// script's `field_C` is 0x10, and otherwise sets `field_F` and starts cap
/// slot 1. The task advances to state 2 on every path.
static void func_neo_ark_shrine_8017EE44(Task* task)
{
    RoomActionPrompt*   prompt = D_80114D28;
    NeoArkShrineScript* work   = (NeoArkShrineScript*)task->work;

    prompt->mode     = 0;
    prompt->targetId = 0;
    func_neo_ark_shrine_8017EAC0(task);
    if (func_800D4EC0() == 0) {
        task->state = 2;
        return;
    }
    if (work->field_C == 0x10) {
        Gp_StartCapSlot(2, 0, 0);
        task->state = 2;
        return;
    }
    work->field_F = 1;
    Gp_StartCapSlot(1, 0, 0);
    task->state = 2;
}

static void func_neo_ark_shrine_8017EED4(Task* task)
{
    D_80114D08 = 0xA;
    Gp_MsgPlayerWeapon(1);
    Gp_MsgPlayer3F3(1);
    Display_ReleaseRef();
    gGameSession->eventState          = 0;
    gGameSession->hideHud             = 0;
    gGameSession->cutsceneHold        = 0;
    Mc_SaveData[0].state.at4.loc.view = 0xA;
    /* Without this the scheduler hoists the `spawnArg2` load above the
       `Mc_SaveData[0].state.at4.loc.view` byte store, which then fills `taskKill`'s delay slot. */
    taskKill((Task*)task->spawnArg2.pointer);
    Task_RequestKill(task, 0);
}

/// Same as `func_neo_ark_shrine_8017F320`, but it latches the script's pad
/// mode on rather than off.
static void func_neo_ark_shrine_8017EF68(Task* task)
{
    RoomActionPrompt*   prompt = D_80114D28;
    NeoArkShrineScript* st     = (NeoArkShrineScript*)task->work;

    Gp_SpawnPadLerp(0x12, 0x30, 0x90);
    D_neo_ark_shrine_80186868 = 1;
    prompt->mode              = 0;
    prompt->targetId          = 0;
    func_neo_ark_shrine_8017EAC0(task);
    st->timer = 0;
    task->state++;
}

/// The script step that runs while the shrine's pad is idle: it re-clears the
/// prompt, ticks the step's timer, and once the step has run 0x1E frames latches
/// the shrine's mode — 2, or 5 when flag 0xE9 is set — into `Mc_SaveData[0].state.at4.loc.room` and the
/// session, which makes the room rebuild its objects, and enters state 2.
///
/// The same literal is stored in both arms on purpose: `gGameSession` is read
/// per arm, and jump_optimize's cross-jumping (post-sched2) merges the arms'
/// identical `sb` pairs into the join. Written with one shared `var_v0` the
/// stores are one pair too but the constant's `li` precedes the address, the
/// merge swallows the `gGameSession` load as well, and the function comes out
/// four insns short.
static void func_neo_ark_shrine_8017EFE4(Task* task)
{
    RoomActionPrompt*   prompt = D_80114D28;
    NeoArkShrineScript* st     = (NeoArkShrineScript*)task->work;

    prompt->mode     = 0;
    prompt->targetId = 0;
    st->timer        = st->timer + 1;
    func_neo_ark_shrine_8017EAC0(task);
    if (st->timer >= 0x1E) {
        if (GameFlag_GetNibble(0xE9) == 0) {
            Mc_SaveData[0].state.at4.loc.room = 2;
            gGameSession->at4.loc.room        = 2;
        } else {
            Mc_SaveData[0].state.at4.loc.room = 5;
            gGameSession->at4.loc.room        = 5;
        }
        gGameSession->roomObjsDirty = 1;
        task->state                 = 2;
    }
}

static void func_neo_ark_shrine_8017F094(Task* task)
{
    NeoArkShrineScript* st;

    st                        = (NeoArkShrineScript*)task->work;
    D_neo_ark_shrine_8018686A = 1;
    func_neo_ark_shrine_8017EAC0();
    taskKill((Task*)task->spawnArg2.pointer);
    st->timer = 0;
    task->state++;
}

static void func_neo_ark_shrine_8017F0F0(Task* task)
{
    NeoArkShrineScript* st;
    u16                 timer;

    st = (NeoArkShrineScript*)task->work;
    func_neo_ark_shrine_8017EAC0();
    timer     = st->timer + 1;
    st->timer = timer;
    if (timer >= 0x1EU) {
        Task_SpawnFromTable(D_neo_ark_shrine_80182508, 1, 0, 0);
        Mc_SaveData[0].state.at4.loc.view = 0xE;
        /* Without this the scheduler hoists the `task->state` reload above the
           `Mc_SaveData[0].state.at4.loc.view` byte store to fill its load-delay slot. */
        st->timer = 0;
        task->state++;
    }
}

static void func_neo_ark_shrine_8017F178(Task* task)
{
    NeoArkShrineScript* st;
    u16                 timer;
    s32                 next;

    st        = (NeoArkShrineScript*)task->work;
    timer     = st->timer + 1;
    st->timer = timer;
    if (timer >= 0x5AU) {
        st->timer = 0;
        if (GameFlag_GetNibble(0xE9) == 0) {
            Task_SpawnFromTable(D_neo_ark_shrine_80182508, 2, 0, 0);
            Mc_SaveData[0].state.at4.loc.view = 0xD;
            GameFlag_SetNibble(0xE9, 1);
            next = task->state + 1;
        } else {
            next = task->state + 2;
        }
        task->state = next;
    }
}

static void func_neo_ark_shrine_8017F21C(Task* task)
{
    NeoArkShrineScript* st;
    u16                 timer;

    st        = (NeoArkShrineScript*)task->work;
    timer     = st->timer + 1;
    st->timer = timer;
    if (timer == 0x1E) {
        Gp_StateF0.field_20 = 1;
    }
    if (st->timer >= 0x3CU) {
        task->state++;
    }
}

static void func_neo_ark_shrine_8017F274(Task* task)
{
    Gp_StateF0.field_20               = 2;
    Mc_SaveData[0].state.at4.loc.room = 6;
    gGameSession->at4.loc.room        = 6;
    gGameSession->roomObjsDirty       = 1;
    Gp_MsgPlayerWeapon(1);
    Gp_MsgPlayer3F3(1);
    Display_ReleaseRef();
    gGameSession->eventState          = 0;
    gGameSession->hideHud             = 0;
    gGameSession->cutsceneHold        = 0;
    Mc_SaveData[0].state.at4.loc.view = 0xA;
    Task_RequestKill(task, 0);
}

/// Runs the shrine's per-step helper and restarts the script's step timer:
/// raises a pad lerp, clears the prompt's highlight state and advances the
/// task to the next state.
static void func_neo_ark_shrine_8017F320(Task* task)
{
    RoomActionPrompt*   prompt = D_80114D28;
    NeoArkShrineScript* st     = (NeoArkShrineScript*)task->work;

    Gp_SpawnPadLerp(0x12, 0x30, 0x90);
    D_neo_ark_shrine_80186868 = 0;
    prompt->mode              = 0;
    prompt->targetId          = 0;
    func_neo_ark_shrine_8017EAC0(task);
    st->timer = 0;
    task->state++;
}

/// Same as `func_neo_ark_shrine_8017EFE4`, but the mode it latches is 1, or 4
/// when flag 0xE9 is set.
static void func_neo_ark_shrine_8017F398(Task* task)
{
    RoomActionPrompt*   prompt = D_80114D28;
    NeoArkShrineScript* st     = (NeoArkShrineScript*)task->work;

    prompt->mode     = 0;
    prompt->targetId = 0;
    st->timer        = st->timer + 1;
    func_neo_ark_shrine_8017EAC0(task);
    if (st->timer >= 0x1E) {
        if (GameFlag_GetNibble(0xE9) == 0) {
            Mc_SaveData[0].state.at4.loc.room = 1;
            gGameSession->at4.loc.room        = 1;
        } else {
            Mc_SaveData[0].state.at4.loc.room = 4;
            gGameSession->at4.loc.room        = 4;
        }
        gGameSession->roomObjsDirty = 1;
        task->state                 = 2;
    }
}

/// Resets the shrine's 16-slot arrangement puzzle to its starting state: clears
/// the two puzzle flags, reloads the work copy of the slot layout from the
/// room's initial-layout table, and re-seeds the slot arrangement with the
/// room's starting order.
void func_neo_ark_shrine_8017F448(void)
{
    NeoArkShrineSlot* dstSlot;
    NeoArkShrineSlot* srcSlot;
    s16*              dstOrder;
    u16*              srcOrder;
    s32               i;
    u16               y;
    u16               order;

    i                         = 0;
    dstSlot                   = D_neo_ark_shrine_8018688C;
    srcSlot                   = D_neo_ark_shrine_8018256C;
    D_neo_ark_shrine_8018686A = 0;
    D_neo_ark_shrine_80186868 = 0;
    do {
        i++;
        dstSlot->x = srcSlot->x;
        y          = srcSlot->y;
        srcSlot++;
        dstSlot->y = y;
        dstSlot++;
    } while (i < 0x10);

    i        = 0;
    dstOrder = D_neo_ark_shrine_8018686C;
    srcOrder = D_neo_ark_shrine_80182410;
    do {
        order = *srcOrder;
        srcOrder++;
        i++;
        *dstOrder = order;
        dstOrder++;
    } while (i < 0x10);
}

/// Second state of the shrine's first falling prop: allocates its 0x48-byte
/// scratch block, republishes the block's light / colour matrices onto the
/// model's `TmdObject`, parks the prop at its starting position parented to the
/// room's view coordinate system, and advances the task to the falling state.
static void func_neo_ark_shrine_8017F4C8(Task* task)
{
    TmdObject*        extra;
    GfxCoord*         coord;
    NeoArkShrineFall* st;

    extra      = task->extra.tmd;
    coord      = extra->coords;
    st         = (NeoArkShrineFall*)memCalloc(sizeof(NeoArkShrineFall), 0);
    task->work = st;
    if (st == NULL) {
        taskKill(task);
        return;
    }
    extra->lightMtx   = &st->light;
    extra->flags      = 0;
    extra->colorMtx   = &st->color;
    coord->parent     = &gGfxViewCoord;
    coord->coord.t[0] = 0x1B58;
    coord->coord.t[1] = -0xBB8;
    coord->coord.t[2] = -0x3E8;
    func_neo_ark_shrine_8017F86C(task);
    task->state++;
}

static void func_neo_ark_shrine_8017F578(Task* task)
{
    NeoArkShrineFall* st;
    GfxCoord*         coord;
    u16               ticks;
    u16               speed;
    u16               delta;
    s32               y;

    st        = (NeoArkShrineFall*)task->work;
    coord     = task->extra.tmd->coords;
    ticks     = st->ticks + 1;
    st->ticks = ticks;
    if ((s16)ticks == 4) {
        Gp_SpawnPadLerp(0x18, 0x40, 0xFF);
        SndEvt_EnqueueType6(0x55150009, 0, 0);
    }
    speed             = st->speed + 1;
    delta             = st->delta + speed;
    st->delta         = delta;
    st->speed         = speed;
    y                 = coord->coord.t[1] + (s16)delta;
    coord->coord.t[1] = y;
    if (y > 0) {
        coord->coord.t[1] = 0;
        task->state++;
    }
    func_neo_ark_shrine_8017F86C(task);
}

static void func_neo_ark_shrine_8017F640(Task* task)
{
    func_neo_ark_shrine_8017F86C(task);
    if (D_neo_ark_shrine_8018686A == 0) {
        task->state++;
    }
}

/// Second state of the shrine's second falling prop: as `func_neo_ark_shrine_8017F4C8`,
/// but parked at the mirror position on the far side of the shrine.
static void func_neo_ark_shrine_8017F688(Task* task)
{
    TmdObject*        extra;
    GfxCoord*         coord;
    NeoArkShrineFall* st;

    extra      = task->extra.tmd;
    coord      = extra->coords;
    st         = (NeoArkShrineFall*)memCalloc(sizeof(NeoArkShrineFall), 0);
    task->work = st;
    if (st == NULL) {
        taskKill(task);
        return;
    }
    extra->lightMtx   = &st->light;
    extra->flags      = 0;
    extra->colorMtx   = &st->color;
    coord->parent     = &gGfxViewCoord;
    coord->coord.t[0] = 0x222E;
    coord->coord.t[1] = -0xBB8;
    coord->coord.t[2] = -0x11C6;
    func_neo_ark_shrine_8017F86C(task);
    task->state++;
}

static void func_neo_ark_shrine_8017F738(Task* task)
{
    NeoArkShrineFall* st;
    GfxCoord*         coord;
    u16               ticks;
    u16               speed;
    u16               delta;
    s32               y;

    st        = (NeoArkShrineFall*)task->work;
    coord     = task->extra.tmd->coords;
    ticks     = st->ticks + 1;
    st->ticks = ticks;
    if ((s16)ticks == 2) {
        SndEvt_EnqueueType6(0x5515000B, 0, 0);
    }
    if ((s16)st->ticks == 0x12) {
        Gp_SpawnPadLerp(0xA, 0xA0, 0xFF);
    }
    speed             = st->speed + 2;
    delta             = st->delta + speed;
    st->delta         = delta;
    st->speed         = speed;
    y                 = coord->coord.t[1] + (s16)delta;
    coord->coord.t[1] = y;
    if (y > 0) {
        coord->coord.t[1] = 0;
        task->state++;
    }
    func_neo_ark_shrine_8017F86C(task);
}

/// Resets both action-prompt slots and steps the task on: zeroes each slot's
/// fixed-point cursor position and its buttons' hold counters, sets
/// `targetId` to 0x100, `field_E` to 0xF and `mode` to 1.
static void func_neo_ark_shrine_8017F80C(Task* task)
{
    RoomActionPrompt* prompt = D_80114D28;
    s32               i;

    for (i = 0; i < 2; i++, prompt++) {
        prompt->field_0                     = 0;
        prompt->field_4                     = 0;
        prompt->targetId                    = 0x100;
        prompt->field_E                     = 0xF;
        prompt->buttons.slots[0].heldFrames = 0;
        prompt->buttons.slots[1].heldFrames = 0;
        prompt->mode                        = 1;
    }
    task->state = task->state + 1;
}

/// Tail every `NeoArkShrineFall` handler runs: clears the prop's root coordinate
/// flag, rebuilds its world matrix, and republishes the translation in
/// `func_800D7A9C`'s format, lowered by 0x320 so the prop draws on the floor.
static void func_neo_ark_shrine_8017F86C(Task* task)
{
    TmdObject* obj;
    GfxCoord*  coord;
    VECTOR     vec;

    obj                 = task->extra.tmd;
    coord               = obj->coords;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    Gp_UpdateCoord(coord);
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1] - 0x320;
    vec.vz = coord->workm.t[2];
    func_800D7A9C(obj, &vec, 0, 3);
}

/// On the task's first tick stores three ids (0x601DF, 0x601FB, 0x60217) into
/// the `D_80115758` / `D_8011572C` / `D_80115750` slots; then, every tick, runs
/// `func_neo_ark_shrine_8017FC14` over the positions the current camera view
/// shows, drawn from one of the room's `SVECTOR` arrays.
void func_neo_ark_shrine_8017F8DC(Task* task)
{
    if (task->state == 0) {
        D_80115758  = 0x601DF;
        D_8011572C  = 0x601FB;
        D_80115750  = 0x60217;
        task->state = 1;
    }

    switch (Gp_GetViewIndex() & 0xFF) {
        case 2: {
            SVECTOR* p = D_neo_ark_shrine_801826D4;
            func_neo_ark_shrine_8017FC14(&p[0], 1, 0x300);
            func_neo_ark_shrine_8017FC14(&p[1], 1, 0x300);
            func_neo_ark_shrine_8017FC14(&p[2], 1, 0x300);
            func_neo_ark_shrine_8017FC14(&p[3], 1, 0x300);
            func_neo_ark_shrine_8017FC14(&p[4], 1, 0x300);
            func_neo_ark_shrine_8017FC14(&p[5], 1, 0x300);
            break;
        }
        case 3: {
            SVECTOR* p = D_neo_ark_shrine_801826AC;
            func_neo_ark_shrine_8017FC14(&p[0], 1, 0x300);
            func_neo_ark_shrine_8017FC14(&p[1], 1, 0x300);
            func_neo_ark_shrine_8017FC14(&p[2], 1, 0x300);
            func_neo_ark_shrine_8017FC14(&p[5], 1, 0x300);
            func_neo_ark_shrine_8017FC14(&p[6], 1, 0x300);
            func_neo_ark_shrine_8017FC14(&p[8], 1, 0x300);
            func_neo_ark_shrine_8017FC14(&p[9], 1, 0x300);
            func_neo_ark_shrine_8017FC14(&p[10], 1, 0x300);
            break;
        }
        case 4: {
            SVECTOR* p = D_neo_ark_shrine_801826AC;
            func_neo_ark_shrine_8017FC14(&p[0], 1, 0x300);
            func_neo_ark_shrine_8017FC14(&p[3], 1, 0x300);
            func_neo_ark_shrine_8017FC14(&p[4], 1, 0x300);
            func_neo_ark_shrine_8017FC14(&p[5], 1, 0x300);
            break;
        }
        case 5:
        case 18: {
            SVECTOR* p = D_neo_ark_shrine_801826AC;
            func_neo_ark_shrine_8017FC14(&p[0], 1, 0x300);
            func_neo_ark_shrine_8017FC14(&p[3], 1, 0x300);
            func_neo_ark_shrine_8017FC14(&p[4], 1, 0x300);
            break;
        }
        case 6: {
            SVECTOR* p = D_neo_ark_shrine_8018269C;
            func_neo_ark_shrine_8017FC14(&p[0], 1, 0x300);
            func_neo_ark_shrine_8017FC14(&p[1], 1, 0x300);
            func_neo_ark_shrine_8017FC14(&p[5], 1, 0x300);
            func_neo_ark_shrine_8017FC14(&p[6], 1, 0x300);
            break;
        }
        case 7: {
            SVECTOR* p = D_neo_ark_shrine_8018268C;
            func_neo_ark_shrine_8017FC14(&p[0], 1, 0x300);
            func_neo_ark_shrine_8017FC14(&p[2], 1, 0x300);
            break;
        }
        case 12: {
            SVECTOR* p = D_neo_ark_shrine_80182694;
            func_neo_ark_shrine_8017FC14(&p[0], 1, 0x300);
            func_neo_ark_shrine_8017FC14(&p[3], 1, 0x300);
            func_neo_ark_shrine_8017FC14(&p[6], 1, 0x300);
            break;
        }
        case 14: {
            SVECTOR* p = D_neo_ark_shrine_801826C4;
            func_neo_ark_shrine_8017FC14(&p[0], 1, 0x300);
            func_neo_ark_shrine_8017FC14(&p[1], 1, 0x300);
            func_neo_ark_shrine_8017FC14(&p[2], 1, 0x300);
            break;
        }
        case 16: {
            SVECTOR* p = D_neo_ark_shrine_801826AC;
            func_neo_ark_shrine_8017FC14(&p[0], 1, 0x300);
            func_neo_ark_shrine_8017FC14(&p[3], 1, 0x300);
            func_neo_ark_shrine_8017FC14(&p[4], 1, 0x300);
            func_neo_ark_shrine_8017FC14(&p[5], 1, 0x300);
            break;
        }
        case 10:
        case 17: {
            SVECTOR* p = D_neo_ark_shrine_80182704;
            func_neo_ark_shrine_8017FC14(&p[0], 0, 0x300);
            func_neo_ark_shrine_8017FC14(&p[1], 0, 0x300);
            break;
        }
    }
}

/// Projects the world-space point `pos` through `gGfxViewCoord.workm` and, when
/// the GTE flag is non-negative, queues one semi-transparent `POLY_FT4` sprite
/// centred on it (tpage 0x2B, clut `(arg1 & 0x3F) | 0x4380`). `arg1` selects
/// the 40-texel UV column `(s16)arg1 * 40` at v=0..0x27, and `arg2` is a signed
/// half-extent whose on-screen radius is `(s16)arg2 * 39 / otz`. All three RGB
/// channels take `0x20`, plus 0x10 on odd `animFrame` values, so the sprite
/// flickers frame to frame.
static void func_neo_ark_shrine_8017FC14(SVECTOR* pos, s32 arg1, s32 arg2)
{
    void**             scratch;
    u8*                head;
    u8*                tmp;
    RoomDraw13Scratch* block;
    POLY_FT4*          prim;
    DisplayState*      ds;
    s32                idx;
    s32                u0;
    s32                u1;
    s32                sarg;
    s32                blend;
    s16                xy;

    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    tmp      = head - 0x10;
    block    = (RoomDraw13Scratch*)tmp;
    *scratch = tmp;

    gte_SetTransMatrix(&gGfxViewCoord.workm);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_ldv0(pos);
    gte_rtps();
    ds    = &gDisplayState;
    blend = (((u8)ds->animFrame & 1) * 16) + 0x20;
    gte_stsxy(&((RoomDraw13Scratch*)(head - 0x10))->sx);
    gte_stflg(&((RoomDraw13Scratch*)(head - 0x10))->flag);
    if (((RoomDraw13Scratch*)tmp)->flag >= 0) {
        gte_stszotz(&block->otz);
        prim           = gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2C);
        idx         = (s16)arg1;
        prim->tpage = 0x2B;
        prim->clut  = (idx & 0x3F) | 0x4380;
        u0          = idx * 40;
        u1          = u0 + 0x27;
        sarg        = (s16)arg2;
        setRGB0(prim, blend, blend, blend);
        prim->u0                          = u0;
        prim->v0                          = 0;
        prim->u1                          = u1;
        prim->v1                          = 0;
        prim->u2                          = u0;
        prim->v2                          = 0x27;
        prim->u3                          = u1;
        prim->v3                          = 0x27;
        prim->code                       |= 2;
        ((RoomDraw13Scratch*)tmp)->radius = (sarg * 40 - sarg) / ((RoomDraw13Scratch*)(head - 0x10))->otz;
        xy                                = ((RoomDraw13Scratch*)tmp)->sx - (u16)((RoomDraw13Scratch*)tmp)->radius;
        prim->x2                          = xy;
        prim->x0                          = xy;
        xy                                = ((RoomDraw13Scratch*)tmp)->sx + (u16)((RoomDraw13Scratch*)tmp)->radius;
        prim->x3                          = xy;
        prim->x1                          = xy;
        xy                                = ((RoomDraw13Scratch*)tmp)->sy - (u16)((RoomDraw13Scratch*)tmp)->radius;
        prim->y1                          = xy;
        prim->y0                          = xy;
        xy                                = ((RoomDraw13Scratch*)tmp)->sy + (u16)((RoomDraw13Scratch*)tmp)->radius;
        prim->y3                          = xy;
        prim->y2                          = xy;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)((RoomDraw13Scratch*)(head - 0x10))->otz << ds->otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                prim);
    }
    SCRATCH_POP_BYTES_AT(scratch, 0x10);
}

/// Effect task drawing a glow at its coordinate's position: over `spawnArg1` frames
/// it grows two starbursts and a ring, flashes the screen when that ends, then
/// shrinks a two-ring billboard until it fades out and releases its work.
void func_neo_ark_shrine_8017FEA0(Task* task)
{
    GpEffWork* work;
    GfxCoord*  coord;
    u8         rgb[3];

    work  = task->spawnArg2.pointer;
    coord = task->extra.coordBody->coord;
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
                func_neo_ark_shrine_80180570(coord, work->angle, rgb);
                rgb[0] >>= 1;
                rgb[1] >>= 1;
                rgb[2] >>= 1;
                func_neo_ark_shrine_80180570(coord, (s16)((u16)work->angle * 2), rgb);
                func_neo_ark_shrine_80180144(coord, (s16)(0x300 - (u16)work->angle * 2), 0x80, rgb);
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
                    func_neo_ark_shrine_80181474(coord, (s16)(work->angle * 3), rgb);
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
/// the GTE flag is non-negative, queues sixteen gouraud `POLY_G4` wedges that
/// form a ring. `arg1` is the inner half-extent and `arg2` the extra outer
/// width; on-screen radii are `(s16)arg1 * 64 / (otz + 1)` and
/// `(s16)(arg1 + arg2) * 64 / (otz + 1)`. The RGB triple tints the inner edge
/// so each wedge fades to a black outer rim.
static void func_neo_ark_shrine_80180144(GfxCoord* arg0, s32 arg1, s32 arg2, u8* rgb)
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

/// Projects the coordinate's world position through `GsWSMATRIX` and, when
/// the GTE flag is non-negative, queues eight gouraud `POLY_G4` wedges around
/// the projected centre. `arg1` is a signed half-extent; the on-screen radius
/// is `(s16)arg1 * 64 / (otz + 1)`. The RGB triple in `rgb` lights only the
/// inner vertex so each wedge fades to black.
static void func_neo_ark_shrine_80180570(GfxCoord* arg0, s16 arg1, u8* rgb)
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

/// Effect task drawing a beam trail: state 0 allocates sixteen coordinates and
/// fills both eight-slot trails from the two start positions; state 1 each
/// frame records the current ends into the next slot and draws the trail, and
/// releases the effect after `spawnArg1` frames.
void func_neo_ark_shrine_80180904(Task* task)
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
                objCoord->coord.t[0]   = D_neo_ark_shrine_80182714[0].vx;
                objCoord->coord.t[1]   = D_neo_ark_shrine_80182714[0].vy;
                objCoord->coord.t[2]   = D_neo_ark_shrine_80182714[0].vz;
                objCoord->composeStamp = GRAPHICS_COORD_DIRTY;
                Gp_UpdateCoord(objCoord);
                task->state        = 1;
                coord.parent       = work->parent;
                vec                = &D_neo_ark_shrine_80182714[1];
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
                    SVECTOR* edge    = &D_neo_ark_shrine_80182714[1];
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
                func_neo_ark_shrine_80180DF4(coords, &coords[8], work->age & 7, 0x123);
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
static void func_neo_ark_shrine_80180DF4(GfxCoord* arg0, GfxCoord* arg1, s16 arg2, s16 arg3)
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

/// Effect task of a burst at its coordinate's position: spawns its particle effects
/// on the first frame, then either sprays sparks in random directions or grows
/// two fading rings for seven frames, and releases its work.
void func_neo_ark_shrine_801811EC(Task* task)
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
            func_neo_ark_shrine_80180144(objCoord, 0x100, 0x100, rgb);
            func_neo_ark_shrine_80180144(objCoord, work->scale, work->scale, rgb);
            if (work->age >= 7) {
                task->state = 3;
            }
            break;

        case 3:
            Gp_ReleaseState1CMem(work, task);
            break;
    }
}

/// Projects the coordinate's world position through `GsWSMATRIX` and, when
/// the GTE flag is non-negative, queues sixteen gouraud `POLY_G4` wedges that
/// form a two-ring billboard. `arg1` is a signed half-extent; on-screen radii
/// are `(s16)arg1 * 64 / (otz + 1)` (outer) and `(s16)arg1 * 8 / (otz + 1)`
/// (inner). The RGB triple tints the inner vertex of the inner ring at full
/// brightness and the outer ring at half, so each wedge fades to a black rim.
static void func_neo_ark_shrine_80181474(GfxCoord* arg0, s16 arg1, u8* arg2)
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
