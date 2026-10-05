#include "rooms/acropolis_fountain.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "gte.h"
#include "common.h"

#include "acropolis_fountain_private.h"

#include "actors/task_tables.h"

#include "gameplay/display.h"
#include "gameplay/actor_render.h"
#include "gameplay/area.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/effects.h"
#include "gameplay/light.h"
#include "gameplay/loading.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"
#include "gameplay/world_collision.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/fs_types.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/stream.h"
#include "main/stream_types.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "mapui/map_akropolis.h"

#include "rooms/room_common.h"

extern WorldCollisionTrigger D_acropolis_fountain_8017F9C0[9];

/// Phases of `_AcropolisFountainWaterLoop`.
///
/// Wait watches the streamed movie. The first time its frame is inside the
/// playing window, and any later time the loop is not already running, the
/// phase advances. Defer holds one tick. Apply commands the loop for a camera
/// that hears the fountain, marks the loop running, and returns to wait.
enum {
    ACROPOLIS_FOUNTAIN_WATER_LOOP_WAIT  = 0,
    ACROPOLIS_FOUNTAIN_WATER_LOOP_DEFER = 1,
    ACROPOLIS_FOUNTAIN_WATER_LOOP_APPLY = 2
};

/// Encoded frames of the fountain movie, numbered from 1.
///
/// The loop may be commanded on `[FIRST, FADE)`. From `FADE` a running loop
/// fades out. `WINDOW` is the exclusive bound of `(frame - FIRST)` and still
/// covers `FADE`; the fade test is taken first.
#define ACROPOLIS_FOUNTAIN_WATER_LOOP_FRAME_FIRST 0xF
#define ACROPOLIS_FOUNTAIN_WATER_LOOP_FRAME_FADE  0xF0
#define ACROPOLIS_FOUNTAIN_WATER_LOOP_WINDOW      0xE2

/// Audio updates over which the loop fades once the movie leaves the window.
#define ACROPOLIS_FOUNTAIN_WATER_LOOP_FADE_TICKS 0x14

/// Task work for the fountain's waterfall loop.
///
/// Allocated into `Task::work` and cleared. `seenWindow` records that this
/// task has already observed the playing window, so it does not command the
/// loop again while that loop is still running. Whether the script is running
/// lives in a separate room global, which survives the task being replaced on
/// a camera cut. A new task has not seen the window, so while the script is
/// still running it re-pans that script instead of starting another.
typedef struct {
    u16 state;      // Phase (0 wait, 1 defer one tick, 2 start or re-pan)
    u16 seenWindow; // Nonzero once this task has observed the playing window
} _AcropolisFountainWaterLoop;
STATIC_ASSERT_SIZEOF(_AcropolisFountainWaterLoop, 4);

extern WorldCollisionTrigger D_acropolis_fountain_8017E7A4;
extern SVECTOR               D_acropolis_fountain_8017E7F0;
extern s16                   D_acropolis_fountain_8017E7F8;
extern TaskDesc              D_acropolis_fountain_8017E7FC[];
extern Task*                 D_acropolis_fountain_80183BB4;

static void func_acropolis_fountain_8017E15C(Task* task, s32 view);

void func_acropolis_fountain_8017E3D4(Task*);
void func_acropolis_fountain_8017E72C(Task*);

extern WorldCollisionGrid    D_acropolis_fountain_8017F60C[1];
extern WorldCollisionTrigger D_acropolis_fountain_8017F630[12];
extern WorldCoordRoomLights  D_acropolis_fountain_8017FF34[1];

extern SpriteBatch  D_acropolis_fountain_8017FF4C[2];
extern SpriteBatch  D_acropolis_fountain_8017FF5C[2];
extern SpriteBatch  D_acropolis_fountain_801806D8[9];
extern SpriteBatch  D_acropolis_fountain_80180DB0[15];
extern SpriteBatch  D_acropolis_fountain_801813DC[8];
extern SpriteBatch  D_acropolis_fountain_8018155C[3];
extern SpriteBatch  D_acropolis_fountain_80181B00[9];
extern SpriteBatch  D_acropolis_fountain_80182304[18];
extern SpriteBatch  D_acropolis_fountain_80182394[2];
extern SpriteBatch  D_acropolis_fountain_801823A4[2];
extern SpriteBatch  D_acropolis_fountain_801827EC[9];
extern SpriteBatch  D_acropolis_fountain_80182F64[15];
extern SpriteBatch  D_acropolis_fountain_801832E8[8];
extern SpriteBatch  D_acropolis_fountain_801833C8[3];
extern SpriteSource D_acropolis_fountain_8017FF6C[95];
extern SpriteSource D_acropolis_fountain_80180720[84];
extern SpriteSource D_acropolis_fountain_80180E28[73];
extern SpriteSource D_acropolis_fountain_8018141C[16];
extern SpriteSource D_acropolis_fountain_80181574[71];
extern SpriteSource D_acropolis_fountain_80181B48[99];
extern SpriteSource D_acropolis_fountain_801823B4[54];
extern SpriteSource D_acropolis_fountain_80182834[92];
extern SpriteSource D_acropolis_fountain_80182FDC[39];
extern SpriteSource D_acropolis_fountain_80183328[8];

WorldCollisionTrigger D_acropolis_fountain_8017E7A4 = { NULL, NULL, NULL, { 2447, -24, -4713, 0 }, { { -699, 0, -405, 0 }, { 750, 0, 93, 0 }, { -749, 0, -108, 0 }, { 700, 0, 422, 0 } }, { 0, 4100, 0, 0 }, { -995, 0, 3973, 0 }, 817, WORLD_COLLISION_TRIGGER_ACTION_CALLBACK, 0, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 };

SVECTOR D_acropolis_fountain_8017E7F0 = { 4000, -688, -6300, 0 };

s16 D_acropolis_fountain_8017E7F8 = 0;

TaskDesc D_acropolis_fountain_8017E7FC[2] = {
    { { { TASK_BODY_NONE, 192 } }, func_acropolis_fountain_8017E3D4, { .value = 0 } },
    { { { TASK_BODY_NONE, 192 } }, func_acropolis_fountain_8017E72C, { .value = 0 } },
};

WorldCollisionRoomResources D_acropolis_fountain_8017E814[2] = {
    { D_acropolis_fountain_8017F60C, D_acropolis_fountain_8017F630, D_acropolis_fountain_8017F9C0, NULL },
    { D_acropolis_fountain_8017F60C, D_acropolis_fountain_8017F630, D_acropolis_fountain_8017F9C0, NULL },
};

u8 D_acropolis_fountain_8017E834[24] = {
    1,
    10,
    11,
    12,
    13,
    14,
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
    22,
    0,
    0,
};

u8* D_acropolis_fountain_8017E84C[2] = {
    gViewIdentityMap,
    D_acropolis_fountain_8017E834,
};

ViewCount D_acropolis_fountain_8017E854[2] = { 22, 22 };

WorldCoordRoomLighting D_acropolis_fountain_8017E858[2] = {
    { D_acropolis_fountain_8017FF34, NULL },
    { D_acropolis_fountain_8017FF34, NULL },
};

DirectionWarpEntry D_acropolis_fountain_8017E868[5] = {
    { { { .word = 2048 }, -3896, 65, 3466 }, { 0, 0, 0, 0 }, { { .word = 2048 }, -3896, 65, 3466 }, { 0, 0, 0, 0 }, 0x51080006, 0x51080005, DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_NONE, 496 },
    { { { .word = 2048 }, 4015, 65, 3348 }, { 0, 0, 0, 0 }, { { .word = 2048 }, 4015, 65, 3348 }, { 0, 0, 0, 0 }, 0x51080006, 0x51080005, DIRECTION_WARP_SOUND_NONE, 6, DIRECTION_WARP_FLAG_NONE, 495 },
    { { { .word = 1024 }, -5162, 66, -5011 }, { 0, 0, 0, 0 }, { { .word = 1024 }, -5162, 66, -5011 }, { 0, 0, 0, 0 }, 0x51080008, 0x51080007, DIRECTION_WARP_SOUND_NONE, 3, DIRECTION_WARP_FLAG_NONE, 497 },
    { { { .word = 2048 }, 4015, 65, 3348 }, { 0, 0, 0, 0 }, { { .word = 2048 }, 4015, 65, 3348 }, { 0, 0, 0, 0 }, 0x51080006, 0x51080005, DIRECTION_WARP_SOUND_NONE, 17, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
    { { { .word = 1024 }, -5162, 66, -5011 }, { 0, 0, 0, 0 }, { { .word = 1024 }, -5162, 66, -5011 }, { 0, 0, 0, 0 }, 0x51080006, 0x51080005, DIRECTION_WARP_SOUND_NONE, 17, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
};

static SVECTOR _gAcropolisFountainCollision0204CNormals[32] = {
#include "assets/acropolis_fountain_collision_0204C_normals.inc"
};

static SVECTOR _gAcropolisFountainCollision0204CVerts[181] = {
#include "assets/acropolis_fountain_collision_0204C_verts.inc"
};

static WorldCollisionGridFace _gAcropolisFountainCollision0204CFaces[66] = {
#include "assets/acropolis_fountain_collision_0204C_faces.inc"
};

static s16 _gAcropolisFountainCollision0204CCells[326] = {
#include "assets/acropolis_fountain_collision_0204C_cells.inc"
};

#define GRID_CELL(i) (&_gAcropolisFountainCollision0204CCells[i])
static s16* _gAcropolisFountainCollision0204CTable[16] = {
#include "assets/acropolis_fountain_collision_0204C_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid D_acropolis_fountain_8017F60C[1] = {
    { NULL, _gAcropolisFountainCollision0204CNormals, _gAcropolisFountainCollision0204CVerts, _gAcropolisFountainCollision0204CFaces, _gAcropolisFountainCollision0204CTable, 6000, 8530, 4, 4, 4000, 66 },
};

WorldCollisionTrigger D_acropolis_fountain_8017F630[12] = {
    { NULL, NULL, NULL, { -3940, -641, -353, 0 }, { { -2214, 2717, -19, 0 }, { 2209, 2724, 16, 0 }, { -2208, -2723, -15, 0 }, { 2215, -2716, 20, 0 } }, { -33, 2, 4104, 0 }, { 0, 4096, 0, 0 }, 3500, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -1762, -832, -3849, 0 }, { { -84, 2813, 1788, 0 }, { 82, 2628, -1783, 0 }, { -83, -2627, 1783, 0 }, { 85, -2812, -1788, 0 } }, { 4114, 0, 190, 0 }, { 0, 4096, 0, 0 }, 3328, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -1570, -800, -3778, 0 }, { { 85, 2813, -1708, 0 }, { -101, 2628, 1675, 0 }, { 84, -2627, -1705, 0 }, { -105, -2812, 1682, 0 } }, { -4096, 0, -226, 0 }, { 0, 4096, 0, 0 }, 3278, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 3359, -1152, -5074, 0 }, { { -1802, 2813, -614, 0 }, { 1798, 2628, 609, 0 }, { -1798, -2627, -610, 0 }, { 1802, -2812, 615, 0 } }, { -1320, 1, 3883, 0 }, { 0, 4096, 0, 0 }, 3396, 0, 5, 7, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 3404, -1120, -4872, 0 }, { { 1836, 2813, 590, 0 }, { -1878, 2628, -631, 0 }, { 1831, -2627, 586, 0 }, { -1884, -2812, -636, 0 } }, { 1283, 1, -3907, 0 }, { 0, 4096, 0, 0 }, 3425, 0, 7, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 1790, -1184, -3860, 0 }, { { -331, 2718, -1824, 0 }, { 330, 2723, 1818, 0 }, { -330, -2722, -1818, 0 }, { 332, -2717, 1824, 0 } }, { -4035, 0, 731, 0 }, { 0, 4096, 0, 0 }, 3288, 0, 4, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 1373, -1152, -3877, 0 }, { { 300, 2718, 1860, 0 }, { -299, 2723, -1853, 0 }, { 301, -2722, 1855, 0 }, { -299, -2717, -1859, 0 } }, { 4046, 0, -654, 0 }, { 0, 4096, 0, 0 }, 3298, 0, 5, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 4732, -960, -100, 0 }, { { 3295, 2718, -60, 0 }, { -3363, 2723, 24, 0 }, { 3292, -2722, -59, 0 }, { -3368, -2717, 29, 0 } }, { -53, -1, -4112, 0 }, { 0, 4096, 0, 0 }, 4283, 0, 5, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 4829, -1152, -579, 0 }, { { -3354, 2718, 33, 0 }, { 3312, 2723, -54, 0 }, { -3349, -2722, 32, 0 }, { 3319, -2717, -56, 0 } }, { 53, -1, 4116, 0 }, { 0, 4096, 0, 0 }, 4283, 0, 6, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -3969, -1056, 31, 0 }, { { 2211, 2717, 128, 0 }, { -2205, 2724, -124, 0 }, { 2205, -2723, 124, 0 }, { -2211, -2716, -128, 0 } }, { 233, 2, -4099, 0 }, { 0, 4096, 0, 0 }, 3500, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 2478, -896, -7177, 0 }, { { -1531, 2813, -306, 0 }, { 1527, 2628, 301, 0 }, { -1526, -2627, -300, 0 }, { 1531, -2812, 306, 0 } }, { -800, 2, 4029, 0 }, { 0, 4096, 0, 0 }, 3207, 0, 7, 8, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 2510, -896, -7050, 0 }, { { 1458, 2813, 294, 0 }, { -1516, 2628, -326, 0 }, { 1453, -2627, 288, 0 }, { -1518, -2812, -327, 0 } }, { 835, 3, -4012, 0 }, { 0, 4096, 0, 0 }, 3197, 0, 8, 7, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionTrigger D_acropolis_fountain_8017F9C0[9] = {
    { NULL, NULL, NULL, { -4000, -16, 3616, 0 }, { { -1024, 0, -1024, 0 }, { 1024, 0, -1024, 0 }, { -1024, 0, 1024, 0 }, { 1024, 0, 1024, 0 } }, { 0, 4096, 0, 0 }, { 0, 0, -4096, 0 }, 1448, WORLD_COLLISION_TRIGGER_ACTION_WARP, 3, 18, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 3968, -16, 3552, 0 }, { { -1024, 0, -1024, 0 }, { 1024, 0, -1024, 0 }, { -1024, 0, 1024, 0 }, { 1024, 0, 1024, 0 } }, { 0, 4096, 0, 0 }, { 0, 0, -4096, 0 }, 1448, WORLD_COLLISION_TRIGGER_ACTION_WARP, 9, 35, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -5408, -16, -5088, 0 }, { { -736, 0, -704, 0 }, { 736, 0, -704, 0 }, { -736, 0, 704, 0 }, { 736, 0, 704, 0 } }, { 0, 4095, 0, 0 }, { 4096, 0, 0, 0 }, 1017, WORLD_COLLISION_TRIGGER_ACTION_WARP, 7, 49, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -4817, -96, -3521, 0 }, { { -459, 0, -1316, 0 }, { 1206, 0, -486, 0 }, { -1205, 0, 487, 0 }, { 460, 0, 1317, 0 } }, { 0, 4104, 0, 0 }, { 0, 0, -4096, 0 }, 1390, WORLD_COLLISION_TRIGGER_ACTION_CAP, 2, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 16, -96, -2848, 0 }, { { -1680, 0, -656, 0 }, { 80, 0, -1424, 0 }, { -48, 0, 304, 0 }, { 1648, 0, -656, 0 } }, { 0, 4108, 0, 0 }, { 0, 0, -4096, 0 }, 1801, WORLD_COLLISION_TRIGGER_ACTION_CAP_WEAPON, 3, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 2415, -24, -4681, 0 }, { { -859, 0, -725, 0 }, { 1070, 0, -67, 0 }, { -1069, 0, 52, 0 }, { 860, 0, 742, 0 } }, { 0, 4100, 0, 0 }, { -995, 0, 3973, 0 }, 1130, WORLD_COLLISION_TRIGGER_ACTION_CAP, 4, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4229, -256, -6624, 0 }, { { -1360, 0, -1312, 0 }, { 176, 0, -1312, 0 }, { -1360, 0, 640, 0 }, { 176, 0, 640, 0 } }, { 0, 4107, 0, 0 }, { 0, 0, -4096, 0 }, 1885, WORLD_COLLISION_TRIGGER_ACTION_CAP_WEAPON, 5, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 3264, -64, -6816, 0 }, { { -400, 0, -832, 0 }, { 400, 0, -832, 0 }, { -400, 0, 832, 0 }, { 400, 0, 832, 0 } }, { 0, 4098, 0, 0 }, { -4096, 0, 0, 0 }, 923, WORLD_COLLISION_TRIGGER_ACTION_CAP_WEAPON, 5, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 3792, -64, -7536, 0 }, { { -544, 0, -368, 0 }, { 544, 0, -368, 0 }, { -544, 0, 368, 0 }, { 544, 0, 368, 0 } }, { 0, 4102, 0, 0 }, { 0, 0, -4096, 0 }, 655, WORLD_COLLISION_TRIGGER_ACTION_CAP_WEAPON, 5, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

AreaResource D_acropolis_fountain_8017FC6C[2] = {
    { 11, 11, AREA_RESOURCE_FILE_GROUP_BASE_30, 0, { 0, 0 }, D_actor_301100_80177400 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_acropolis_fountain_8017FC84[2] = {
    { 49, 49, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, D_actor_101100_80147400 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaVariant D_acropolis_fountain_8017FC9C[11] = {
    { NULL, NULL },
    { NULL, NULL },
    { D_map_akropolis_8017B17C, D_acropolis_fountain_8017FC6C },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { D_map_akropolis_8017B1AC, D_acropolis_fountain_8017FC84 },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
};

/// Point lights for model shading and light queries in both fountain room variants.
///
/// All six contribute in every view. Positions and falloff radii use world units;
/// RGB intensities use 12 fractional bits. The loaded overlay owns these mutable
/// records; coordinate updates and lighting queries replace their cached state.
static WorldCoordPointLight _gAcropolisFountainPointLights[] = {
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = {
                                          .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } },
                                          .t = { -4000, -200, 1560 },
                               },
                               .composed = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } } },
                               .viewId   = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                           } },
            .color     = { 2867, 2785, 2621 },
        },
        .inner = 10,
        .outer = 8000,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = {
                                          .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } },
                                          .t = { -3010, -200, -3000 },
                               },
                               .composed = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } } },
                               .viewId   = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                           } },
            .color     = { 2867, 2785, 2621 },
        },
        .inner = 10,
        .outer = 8000,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = {
                                          .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } },
                                          .t = { 10, -200, -4360 },
                               },
                               .composed = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } } },
                               .viewId   = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                           } },
            .color     = { 2867, 2785, 2621 },
        },
        .inner = 10,
        .outer = 8000,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = {
                                          .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } },
                                          .t = { 3830, -200, -2200 },
                               },
                               .composed = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } } },
                               .viewId   = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                           } },
            .color     = { 2867, 2785, 2621 },
        },
        .inner = 10,
        .outer = 8000,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = {
                                          .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } },
                                          .t = { 3990, -200, 1620 },
                               },
                               .composed = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } } },
                               .viewId   = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                           } },
            .color     = { 2867, 2785, 2621 },
        },
        .inner = 10,
        .outer = 8000,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = {
                                          .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } },
                                          .t = { 3840, -980, -6510 },
                               },
                               .composed = { .m = { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } } },
                               .viewId   = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                           } },
            .color     = { 2703, 2785, 2621 },
        },
        .inner = 10,
        .outer = 7500,
    },
};

WorldCoordRoomLights D_acropolis_fountain_8017FF34[1] = {
    { 0, NULL, ARRAY_SIZE(_gAcropolisFountainPointLights), _gAcropolisFountainPointLights, 0, NULL },
};

SpriteBatch D_acropolis_fountain_8017FF4C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_acropolis_fountain_8017FF5C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_acropolis_fountain_8017FF6C[95] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -120, -80, 559, { .fields = { 8, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -160, -112, 602, { .fields = { 88, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -160, -80, 748, { .fields = { 88, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -160, -48, 993, { .fields = { 88, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -136, -48, 895, { .fields = { 104, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -136, -80, 570, { .fields = { 112, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -136, -104, 1193, { .fields = { 56, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -152, 16, 739, { .fields = { 112, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -152, -8, 663, { .fields = { 56, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -160, -16, 740, { .fields = { 0, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -104, -72, 448, { .fields = { 16, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -120, -16, 1230, { .fields = { 16, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -120, 24, 720, { .fields = { 16, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -160, 32, 588, { .fields = { 16, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -160, 40, 383, { .fields = { 16, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -152, 40, 435, { .fields = { 16, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -144, 40, 454, { .fields = { 16, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -136, 40, 606, { .fields = { 16, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -136, 48, 412, { .fields = { 16, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -128, 48, 908, { .fields = { 16, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -160, 72, 407, { .fields = { 16, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -160, 80, 403, { .fields = { 16, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -160, 88, 415, { .fields = { 16, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -160, 96, 403, { .fields = { 16, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -160, 104, 403, { .fields = { 32, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -160, -32, 747, { .fields = { 56, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -136, -32, 745, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -160, -64, 727, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -152, -80, 910, { .fields = { 56, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -160, -120, 598, { .fields = { 104, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -136, -120, 576, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -136, -80, 553, { .fields = { 24, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -136, -64, 1185, { .fields = { 80, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -112, -64, 481, { .fields = { 72, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -112, -96, 645, { .fields = { 88, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -96, -72, 480, { .fields = { 112, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -80, -80, 505, { .fields = { 72, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -80, -48, 519, { .fields = { 104, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -112, -32, 488, { .fields = { 0, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -112, 16, 736, { .fields = { 32, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -144, 48, 538, { .fields = { 40, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -144, 104, 438, { .fields = { 56, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -160, 72, 395, { .fields = { 64, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -136, 80, 365, { .fields = { 40, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -160, 56, 409, { .fields = { 24, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -160, 40, 513, { .fields = { 80, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -136, 56, 534, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -120, 8, 636, { .fields = { 64, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -160, 24, 380, { .fields = { 8, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -160, -8, 654, { .fields = { 88, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -136, 24, 440, { .fields = { 48, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -136, 0, 1153, { .fields = { 56, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -104, 56, 650, { .fields = { 16, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -48, 64, 781, { .fields = { 16, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -64, 40, 806, { .fields = { 24, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -64, 32, 881, { .fields = { 0, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, -96, 40, 784, { .fields = { 120, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -80, 32, 857, { .fields = { 24, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, -96, 88, 454, { .fields = { 24, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -64, 88, 550, { .fields = { 72, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 24 } }, -136, 88, 454, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -120, 64, 386, { .fields = { 32, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, -96, 64, 418, { .fields = { 24, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -64, 64, 670, { .fields = { 40, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -96, 48, 698, { .fields = { 0, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -64, 48, 777, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 88, 8, 1212, { .fields = { 8, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 128, 56, 825, { .fields = { 48, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 112, 56, 850, { .fields = { 72, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 96, 56, 912, { .fields = { 80, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 88, 56, 987, { .fields = { 96, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 88, 32, 950, { .fields = { 56, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 96, 32, 925, { .fields = { 40, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 112, 32, 850, { .fields = { 48, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 128, 32, 837, { .fields = { 24, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 128, 16, 900, { .fields = { 0, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 112, 16, 900, { .fields = { 24, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 96, 16, 912, { .fields = { 72, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 88, 16, 925, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, 128, 8, 1275, { .fields = { 112, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, 128, 0, 1287, { .fields = { 112, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 112, 8, 1250, { .fields = { 0, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 112, 0, 1275, { .fields = { 0, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 96, 8, 1237, { .fields = { 0, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 96, 0, 1237, { .fields = { 0, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 80, 56, 1025, { .fields = { 64, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 80, 32, 1037, { .fields = { 64, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 96, 88, 862, { .fields = { 24, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 112, 88, 775, { .fields = { 24, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 128, 88, 775, { .fields = { 8, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, 128, 104, 750, { .fields = { 112, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 136, 32, 850, { .fields = { 8, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 144, 32, 850, { .fields = { 8, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 144, -8, 850, { .fields = { 112, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 152, -24, 850, { .fields = { 16, 16 } }, 128, 128, 128, 0 },
};

SpriteBatch D_acropolis_fountain_801806D8[9] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 10, 0, 0, { 3, 0 } },
    { 10, 30, 0, 0, { 4, 0 } },
    { 40, 7, 0, 0, { 1, 0 } },
    { 47, 5, 0, 0, { 5, 0 } },
    { 52, 14, 0, 0, { 0, 0 } },
    { 66, 25, 0, 0, { 6, 0 } },
    { 91, 4, 0, 0, { 2, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_acropolis_fountain_80180720[84] = {
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -160, 72, 375, { .fields = { 104, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, -160, 80, 375, { .fields = { 88, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, -120, 80, 375, { .fields = { 96, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, -80, 80, 375, { .fields = { 80, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, -40, 80, 375, { .fields = { 56, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -136, 64, 375, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, -120, 56, 375, { .fields = { 40, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, -80, 56, 375, { .fields = { 40, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, 64, 96, 375, { .fields = { 32, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, 120, 80, 375, { .fields = { 88, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, 120, 72, 375, { .fields = { 40, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, 96, 80, 375, { .fields = { 8, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 104, 112, 375, { .fields = { 48, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, 120, -56, 375, { .fields = { 120, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 120, -96, 375, { .fields = { 0, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 96, -16, 375, { .fields = { 32, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 152, 56, 375, { .fields = { 0, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, 80, -96, 375, { .fields = { 112, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, 80, -56, 375, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 64, -48, 375, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, 24, -120, 375, { .fields = { 32, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 32 } }, 56, -120, 375, { .fields = { 56, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 88, -120, 375, { .fields = { 8, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 32 } }, 128, -120, 375, { .fields = { 80, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -160, 0, 500, { .fields = { 48, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -160, -120, 500, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, -120, -120, 500, { .fields = { 24, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 32 } }, -160, -80, 500, { .fields = { 48, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, -160, -48, 500, { .fields = { 64, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -96, 24, 1663, { .fields = { 8, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -104, 32, 1875, { .fields = { 72, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -88, 72, 1637, { .fields = { 88, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -72, 32, 1531, { .fields = { 56, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -72, 72, 1562, { .fields = { 40, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -40, 32, 1525, { .fields = { 64, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, -40, 72, 1500, { .fields = { 8, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 96, 32, 1625, { .fields = { 96, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 96, 72, 1562, { .fields = { 16, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 72, 32, 1612, { .fields = { 104, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, 72, 72, 1612, { .fields = { 24, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, 32, 72, 1562, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, -8, 72, 1500, { .fields = { 8, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -8, 32, 1525, { .fields = { 88, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 32, 32, 1531, { .fields = { 72, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -72, 16, 1625, { .fields = { 88, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -72, -80, 1625, { .fields = { 88, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, -48, -80, 1625, { .fields = { 40, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -64, -56, 1625, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -64, -16, 1625, { .fields = { 56, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, -48, 8, 1625, { .fields = { 24, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, -72, 24, 1625, { .fields = { 48, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 24, -24, 1875, { .fields = { 80, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -16, -16, 1875, { .fields = { 8, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 24, -16, 1875, { .fields = { 40, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 32 } }, -16, -48, 1875, { .fields = { 88, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -16, -72, 1875, { .fields = { 72, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, -16, 24, 1875, { .fields = { 0, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 24, 24, 1875, { .fields = { 16, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 72, -104, 1325, { .fields = { 32, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, 112, -104, 1325, { .fields = { 48, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 88, 40, 1325, { .fields = { 0, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, 128, 64, 1325, { .fields = { 72, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 112, 32, 1325, { .fields = { 16, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 88, 0, 1325, { .fields = { 24, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 88, -40, 1325, { .fields = { 32, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 88, -80, 1325, { .fields = { 48, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 32 } }, 88, -112, 1325, { .fields = { 64, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 96, 24, 1700, { .fields = { 40, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 32 } }, 96, -8, 1700, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 96, -40, 1700, { .fields = { 72, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 32 } }, 88, -72, 1700, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, 128, 96, 375, { .fields = { 80, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, 88, 104, 375, { .fields = { 0, 216 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 16 } }, 48, 104, 375, { .fields = { 120, 16 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, -32, 104, 375, { .fields = { 8, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, 8, 104, 375, { .fields = { 0, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, -72, 104, 375, { .fields = { 0, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, -112, 96, 375, { .fields = { 16, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 32 } }, -144, 88, 375, { .fields = { 64, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -160, 80, 375, { .fields = { 48, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -72, -104, 1250, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 72, 16, 2212, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 88, 16, 2162, { .fields = { 112, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 104, 24, 2100, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
};

SpriteBatch D_acropolis_fountain_80180DB0[15] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 8, 0, 0, { 5, 0 } },
    { 8, 5, 0, 0, { 7, 0 } },
    { 13, 7, 0, 0, { 1, 0 } },
    { 20, 4, 0, 0, { 8, 0 } },
    { 24, 5, 0, 0, { 0, 0 } },
    { 29, 15, 0, 0, { 9, 0 } },
    { 44, 7, 0, 0, { 6, 0 } },
    { 51, 7, 0, 0, { 10, 0 } },
    { 58, 9, 0, 0, { 3, 0 } },
    { 67, 4, 0, 0, { 11, 0 } },
    { 71, 9, 0, 0, { 2, 0 } },
    { 80, 1, 0, 0, { 12, 0 } },
    { 81, 3, 0, 0, { 4, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_acropolis_fountain_80180E28[73] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -80, 56, 750, { .fields = { 72, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -24, 80, 912, { .fields = { 88, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -32, 80, 881, { .fields = { 96, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -48, 80, 825, { .fields = { 88, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -72, 80, 778, { .fields = { 104, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, -32, 48, 875, { .fields = { 88, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, -32, 64, 882, { .fields = { 88, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -48, 40, 1070, { .fields = { 40, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -48, 48, 937, { .fields = { 88, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -48, 64, 825, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -72, 40, 1033, { .fields = { 48, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, -72, 48, 950, { .fields = { 72, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, -72, 64, 778, { .fields = { 72, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, -104, 64, 750, { .fields = { 64, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -104, 80, 750, { .fields = { 56, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -160, 32, 1050, { .fields = { 40, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -120, 32, 1050, { .fields = { 32, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -80, 32, 1000, { .fields = { 32, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -80, 72, 1000, { .fields = { 0, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 32 } }, -120, 72, 1050, { .fields = { 96, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 24 } }, -160, 72, 1050, { .fields = { 80, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -136, 96, 1050, { .fields = { 80, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -56, 0, 1125, { .fields = { 32, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -56, -40, 1125, { .fields = { 32, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -32, -40, 1137, { .fields = { 32, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -56, -80, 1125, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, -32, -56, 1125, { .fields = { 96, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -56, -120, 1125, { .fields = { 8, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, -64, -120, 1125, { .fields = { 96, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -32, -120, 1125, { .fields = { 88, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -80, 40, 1125, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -32, 40, 1150, { .fields = { 104, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -32, 0, 1137, { .fields = { 56, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -144, -96, 1280, { .fields = { 120, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -160, -96, 1199, { .fields = { 120, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -160, -120, 1180, { .fields = { 104, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -144, -120, 1233, { .fields = { 112, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, -128, -112, 1279, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, -128, -96, 1278, { .fields = { 96, 16 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -160, 16, 1234, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -160, -16, 1240, { .fields = { 120, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -160, -48, 1254, { .fields = { 120, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -160, -64, 1260, { .fields = { 96, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -120, -104, 1456, { .fields = { 96, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -152, 32, 1274, { .fields = { 104, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -128, -16, 1470, { .fields = { 80, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -128, -56, 1479, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -144, -96, 1246, { .fields = { 96, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 32 } }, -136, 24, 1343, { .fields = { 112, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 32 } }, -128, 24, 1339, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -128, -96, 1500, { .fields = { 8, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -104, -96, 1538, { .fields = { 0, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 80, -88, 1135, { .fields = { 72, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 72, -80, 932, { .fields = { 40, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 56, -24, 959, { .fields = { 56, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 128, 32, 1212, { .fields = { 64, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 144, 64, 872, { .fields = { 8, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 136, 32, 921, { .fields = { 8, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 136, 0, 925, { .fields = { 8, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 136, -40, 506, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 136, -80, 1179, { .fields = { 104, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 136, -120, 997, { .fields = { 64, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 128, 0, 922, { .fields = { 24, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 104, -80, 921, { .fields = { 64, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 104, -40, 862, { .fields = { 72, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, 80, -40, 1088, { .fields = { 88, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 80, -80, 932, { .fields = { 104, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 64, -40, 851, { .fields = { 88, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, 40, -40, 1569, { .fields = { 72, 16 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 64, -64, 1181, { .fields = { 96, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, 40, -56, 854, { .fields = { 72, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 104, -120, 1052, { .fields = { 96, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 88, -104, 1196, { .fields = { 104, 224 } }, 128, 128, 128, 0 },
};

SpriteBatch D_acropolis_fountain_801813DC[8] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 15, 0, 0, { 3, 0 } },
    { 15, 7, 0, 0, { 0, 0 } },
    { 22, 11, 0, 0, { 5, 0 } },
    { 33, 10, 0, 0, { 1, 0 } },
    { 43, 9, 0, 0, { 4, 0 } },
    { 52, 21, 0, 0, { 2, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_acropolis_fountain_8018141C[16] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -56, 48, 675, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -128, 80, 665, { .fields = { 96, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -96, 80, 667, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -64, 80, 665, { .fields = { 104, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -128, 40, 665, { .fields = { 96, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -96, 40, 665, { .fields = { 96, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -64, 40, 675, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -120, 24, 665, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -112, 0, 665, { .fields = { 112, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -96, 0, 665, { .fields = { 80, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -112, -40, 665, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -96, -40, 665, { .fields = { 72, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -112, -80, 665, { .fields = { 72, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -96, -80, 665, { .fields = { 88, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -112, -120, 665, { .fields = { 80, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -96, -120, 665, { .fields = { 72, 200 } }, 128, 128, 128, 0 },
};

SpriteBatch D_acropolis_fountain_8018155C[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 16, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_acropolis_fountain_80181574[71] = {
    { 141, 0x3FC0, { .fields = { 40, 40 } }, -160, 48, 875, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, -120, 48, 912, { .fields = { 32, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -120, 40, 912, { .fields = { 88, 248 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 40 } }, -160, 8, 875, { .fields = { 120, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, -160, -32, 875, { .fields = { 16, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, -160, -120, 875, { .fields = { 8, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, -160, -80, 875, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 40 } }, -120, -80, 912, { .fields = { 104, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, -160, 8, 1000, { .fields = { 0, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, -120, 8, 997, { .fields = { 48, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, -80, 8, 1025, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -40, 8, 1037, { .fields = { 80, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -24, 8, 1037, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, -160, -32, 1000, { .fields = { 16, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, -120, -32, 1000, { .fields = { 40, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, -80, -32, 1037, { .fields = { 40, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -40, -32, 1037, { .fields = { 0, 80 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 40 } }, -160, -72, 1000, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 40 } }, -120, -72, 1000, { .fields = { 64, 80 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 40 } }, -80, -72, 1037, { .fields = { 48, 200 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 24, 40 } }, -40, -72, 1037, { .fields = { 104, 80 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 40 } }, -160, -112, 1000, { .fields = { 96, 120 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 40 } }, -120, -112, 1000, { .fields = { 80, 160 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 40 } }, -80, -112, 1037, { .fields = { 88, 200 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 24, 40 } }, -40, -112, 1037, { .fields = { 80, 40 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 40 } }, 48, -120, 750, { .fields = { 40, 40 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 40 } }, 48, -80, 750, { .fields = { 40, 160 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 40 } }, 48, -40, 750, { .fields = { 56, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 48, 0, 750, { .fields = { 48, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 48, 40, 750, { .fields = { 56, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 88, 0, 750, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 88, -40, 750, { .fields = { 48, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 88, -80, 750, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 16, -120, 750, { .fields = { 32, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 16, -80, 750, { .fields = { 40, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 16, -40, 750, { .fields = { 72, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 16, 0, 750, { .fields = { 96, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -8, -120, 800, { .fields = { 104, 40 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 32 } }, 0, -80, 800, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 16, 40, 750, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -8, 0, 812, { .fields = { 104, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 88, 40, 750, { .fields = { 64, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 88, 0, 750, { .fields = { 88, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 88, -40, 750, { .fields = { 88, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, 88, -80, 750, { .fields = { 88, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, 48, 0, 750, { .fields = { 96, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, 48, -40, 750, { .fields = { 96, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, 48, -80, 750, { .fields = { 80, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, 48, -120, 750, { .fields = { 56, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 48, 40, 750, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 32 } }, 8, 40, 750, { .fields = { 0, 144 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 24, 32 } }, -16, 40, 800, { .fields = { 0, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, 8, 0, 750, { .fields = { 56, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, -16, 0, 800, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, 8, -40, 750, { .fields = { 96, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 8, -80, 750, { .fields = { 8, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 8, -120, 750, { .fields = { 8, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -32, -120, 800, { .fields = { 16, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -32, -80, 800, { .fields = { 0, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, -16, -40, 800, { .fields = { 120, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -32, -40, 800, { .fields = { 72, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 40 } }, 96, 16, 700, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 104, 24, 188, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 104, 32, 188, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 32 } }, -160, 40, 997, { .fields = { 8, 200 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 32 } }, -120, 40, 997, { .fields = { 0, 32 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 32 } }, -80, 40, 997, { .fields = { 16, 112 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 32 } }, -40, 40, 997, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 24 } }, 0, 40, 997, { .fields = { 8, 232 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 32 } }, 40, 32, 997, { .fields = { 24, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 80, 24, 997, { .fields = { 8, 40 } }, 128, 128, 128, 0 },
};

SpriteBatch D_acropolis_fountain_80181B00[9] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 8, 0, 0, { 3, 0 } },
    { 8, 17, 0, 0, { 4, 0 } },
    { 25, 17, 0, 0, { 1, 0 } },
    { 42, 19, 0, 0, { 5, 0 } },
    { 61, 1, 0, 0, { 0, 0 } },
    { 62, 9, 0, 0, { 6, 0 } },
    { 71, 0, 0, 0, { 2, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_acropolis_fountain_80181B48[99] = {
    { 141, 0x3FC0, { .fields = { 32, 32 } }, 8, 48, 575, { .fields = { 40, 144 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 32, 32 } }, 40, 48, 575, { .fields = { 48, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 88, 16, 572, { .fields = { 88, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 72, -88, 562, { .fields = { 32, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 120, 40, 500, { .fields = { 48, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 120, 0, 500, { .fields = { 48, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, 120, -40, 500, { .fields = { 72, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, 120, -80, 500, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, 120, -120, 500, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 24, 24 } }, 96, 16, 562, { .fields = { 48, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, 80, -120, 562, { .fields = { 88, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, 80, -80, 562, { .fields = { 64, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 40 } }, 88, -40, 562, { .fields = { 88, 200 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 32, 24 } }, 88, 40, 562, { .fields = { 16, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 80, 40, 562, { .fields = { 72, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 64, -80, 562, { .fields = { 56, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 120, 0, 500, { .fields = { 24, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 120, -40, 500, { .fields = { 32, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 120, -80, 500, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 120, -120, 500, { .fields = { 88, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 80, 0, 500, { .fields = { 88, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 80, -40, 500, { .fields = { 88, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 80, -80, 500, { .fields = { 88, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, 80, -120, 500, { .fields = { 40, 120 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 24 } }, 80, 40, 500, { .fields = { 8, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 48, -80, 500, { .fields = { 16, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 40 } }, 48, 0, 500, { .fields = { 96, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 40 } }, 48, -40, 500, { .fields = { 104, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 40 } }, 48, -120, 500, { .fields = { 112, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 16, -80, 500, { .fields = { 0, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 40 } }, 16, -120, 500, { .fields = { 120, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 40 } }, -16, -80, 808, { .fields = { 56, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -24, -80, 761, { .fields = { 80, 240 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 24, 24 } }, 56, 40, 500, { .fields = { 40, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, 40, -80, 1125, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 40, -40, 1125, { .fields = { 8, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 40, 0, 1125, { .fields = { 0, 120 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 32, 24 } }, 80, 0, 1050, { .fields = { 16, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, 80, -80, 1050, { .fields = { 64, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, 40, -40, 1125, { .fields = { 24, 80 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 40 } }, 40, -80, 1125, { .fields = { 120, 40 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 24, 32 } }, 80, -40, 1125, { .fields = { 48, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, 80, -80, 1125, { .fields = { 32, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, 120, 24, 1225, { .fields = { 24, 200 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 32, 32 } }, 88, 16, 1225, { .fields = { 40, 176 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 32, 24 } }, 56, 16, 1225, { .fields = { 40, 208 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 24 } }, 64, 96, 250, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 24 } }, 24, 96, 250, { .fields = { 24, 24 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 24 } }, -16, 96, 250, { .fields = { 8, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, -56, 104, 250, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -88, 104, 250, { .fields = { 40, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -128, 112, 250, { .fields = { 0, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 120, 80, 461, { .fields = { 40, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 120, 40, 455, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, 128, 0, 446, { .fields = { 16, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 128, -40, 437, { .fields = { 8, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 128, -80, 432, { .fields = { 8, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 128, -120, 427, { .fields = { 0, 80 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 40 } }, 80, 80, 508, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 40 } }, 80, 40, 498, { .fields = { 120, 160 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 40 } }, 80, 0, 491, { .fields = { 120, 200 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 40 } }, 80, -40, 726, { .fields = { 112, 80 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 8, 40 } }, 88, -80, 474, { .fields = { 80, 40 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 40 } }, 88, -120, 250, { .fields = { 88, 120 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 40 } }, 40, 80, 250, { .fields = { 88, 40 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 8, 40 } }, 48, 40, 250, { .fields = { 120, 120 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 8, 40 } }, 48, 0, 250, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 48, -40, 250, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 48, -80, 250, { .fields = { 80, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 48, -120, 250, { .fields = { 64, 200 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 40 } }, 8, 80, 250, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 40 } }, 8, 40, 250, { .fields = { 104, 40 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 40 } }, 8, 0, 250, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 40 } }, 8, -40, 250, { .fields = { 64, 40 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 40 } }, 8, -80, 250, { .fields = { 88, 200 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 40 } }, 8, -120, 250, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 40 } }, -32, 40, 250, { .fields = { 104, 120 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 40 } }, -32, 0, 250, { .fields = { 104, 160 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 40 } }, -32, -40, 250, { .fields = { 72, 160 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 40 } }, -32, -80, 250, { .fields = { 72, 200 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 40 } }, -32, -120, 250, { .fields = { 72, 120 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 40 } }, -32, 80, 250, { .fields = { 88, 160 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 40 } }, -72, 80, 250, { .fields = { 104, 200 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 40 } }, -72, 40, 250, { .fields = { 96, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -72, 0, 250, { .fields = { 0, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, -72, -80, 250, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -72, -40, 250, { .fields = { 16, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, -72, -120, 250, { .fields = { 32, 120 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 40 } }, -112, 80, 250, { .fields = { 80, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -112, 40, 250, { .fields = { 24, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, -112, 0, 250, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -120, -40, 250, { .fields = { 72, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -120, -80, 250, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -120, -120, 250, { .fields = { 80, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -152, 80, 250, { .fields = { 72, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -160, 40, 250, { .fields = { 40, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -160, 0, 250, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -160, -40, 250, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -160, -80, 25, { .fields = { 120, 200 } }, 128, 128, 128, 0 },
};

SpriteBatch D_acropolis_fountain_80182304[18] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 2, 0, 0, { 13, 0 } },
    { 2, 1, 0, 0, { 2, 0 } },
    { 3, 13, 0, 0, { 9, 0 } },
    { 16, 18, 0, 0, { 3, 0 } },
    { 34, 5, 0, 0, { 11, 0 } },
    { 39, 4, 0, 0, { 0, 0 } },
    { 43, 3, 0, 0, { 12, 0 } },
    { 46, 6, 0, 0, { 1, 0 } },
    { 52, 6, 0, 0, { 8, 0 } },
    { 58, 6, 0, 0, { 6, 0 } },
    { 64, 6, 0, 0, { 15, 0 } },
    { 70, 6, 0, 0, { 7, 0 } },
    { 76, 6, 0, 0, { 10, 0 } },
    { 82, 6, 0, 0, { 4, 0 } },
    { 88, 6, 0, 0, { 14, 0 } },
    { 94, 5, 0, 0, { 5, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_acropolis_fountain_80182394[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_acropolis_fountain_801823A4[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_acropolis_fountain_801823B4[54] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -120, -80, 1000, { .fields = { 56, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -160, -112, 1000, { .fields = { 88, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -160, -56, 1000, { .fields = { 72, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -144, -104, 1000, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -144, -56, 1000, { .fields = { 80, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -120, -40, 1000, { .fields = { 72, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -152, -8, 1000, { .fields = { 32, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -152, 24, 1000, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -160, 32, 750, { .fields = { 40, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -160, 40, 383, { .fields = { 40, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -152, 40, 435, { .fields = { 80, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -144, 40, 454, { .fields = { 104, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -136, 40, 606, { .fields = { 64, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -136, 48, 412, { .fields = { 72, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -128, 48, 908, { .fields = { 8, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -160, 72, 407, { .fields = { 0, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -160, 80, 403, { .fields = { 0, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -160, 88, 415, { .fields = { 8, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -160, 96, 403, { .fields = { 16, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -160, 104, 403, { .fields = { 24, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 40 } }, -160, -120, 750, { .fields = { 32, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -112, -96, 750, { .fields = { 48, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, -152, -80, 750, { .fields = { 120, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 32 } }, -160, -64, 750, { .fields = { 8, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -112, -72, 750, { .fields = { 56, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -80, -80, 750, { .fields = { 56, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 32 } }, -160, -32, 750, { .fields = { 120, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -120, 16, 750, { .fields = { 8, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -160, 40, 500, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -160, 72, 500, { .fields = { 56, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -136, 80, 500, { .fields = { 32, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 24 } }, -160, -8, 250, { .fields = { 120, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 24 } }, -160, 16, 250, { .fields = { 0, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, -80, 32, 875, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, -96, 40, 850, { .fields = { 8, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -48, 40, 875, { .fields = { 64, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -56, 40, 850, { .fields = { 80, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -64, 64, 825, { .fields = { 88, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -72, 104, 800, { .fields = { 48, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, -104, 56, 825, { .fields = { 112, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 16 } }, -112, 64, 800, { .fields = { 112, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 24 } }, -128, 80, 775, { .fields = { 96, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 72, 16 } }, -144, 104, 750, { .fields = { 88, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 136, 0, 1075, { .fields = { 40, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 112, 0, 1050, { .fields = { 48, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 96, 0, 1037, { .fields = { 40, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 88, 8, 962, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 80, 32, 962, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, 96, 32, 912, { .fields = { 112, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 88 } }, 112, 32, 887, { .fields = { 104, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 88 } }, 136, 32, 875, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 136, 32, 1000, { .fields = { 8, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 152, -24, 1000, { .fields = { 104, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 144, -8, 1000, { .fields = { 96, 168 } }, 128, 128, 128, 0 },
};

SpriteBatch D_acropolis_fountain_801827EC[9] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 8, 0, 0, { 3, 0 } },
    { 8, 20, 0, 0, { 4, 0 } },
    { 28, 3, 0, 0, { 1, 0 } },
    { 31, 2, 0, 0, { 5, 0 } },
    { 33, 10, 0, 0, { 0, 0 } },
    { 43, 8, 0, 0, { 6, 0 } },
    { 51, 3, 0, 0, { 2, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_acropolis_fountain_80182834[92] = {
    { 142, 0x3FC0, { .fields = { 32, 48 } }, -160, 72, 500, { .fields = { 96, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 56 } }, -128, 64, 500, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 56 } }, -88, 64, 500, { .fields = { 40, 184 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 8 } }, -104, 56, 500, { .fields = { 104, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -48, 72, 500, { .fields = { 32, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 48 } }, 120, 72, 500, { .fields = { 96, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, 96, 80, 500, { .fields = { 48, 144 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 32, 24 } }, 64, 96, 500, { .fields = { 112, 224 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 8, 8 } }, 152, -8, 500, { .fields = { 120, 192 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 8, 8 } }, 152, 56, 1389, { .fields = { 120, 216 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 8, 8 } }, 152, 64, 1215, { .fields = { 112, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 88 } }, 128, -104, 500, { .fields = { 64, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 96 } }, 96, -104, 500, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 32 } }, 64, -56, 500, { .fields = { 16, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 80, -80, 500, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 88, -112, 500, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 32 } }, 24, -120, 500, { .fields = { 8, 40 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 24 } }, 72, -120, 500, { .fields = { 104, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 32 } }, 112, -120, 500, { .fields = { 0, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -160, 0, 502, { .fields = { 16, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, -160, -120, 500, { .fields = { 8, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, -120, -120, 500, { .fields = { 64, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 40 } }, -160, -72, 500, { .fields = { 56, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, -104, 48, 1625, { .fields = { 72, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -96, 24, 1575, { .fields = { 88, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -80, 24, 1612, { .fields = { 48, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -96, 40, 1525, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, -80, 40, 1445, { .fields = { 48, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -64, 16, 1675, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 24, 24 } }, -40, 16, 1675, { .fields = { 112, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, -56, 40, 1437, { .fields = { 8, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 48 } }, -24, 40, 1375, { .fields = { 80, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 48 } }, 16, 40, 1375, { .fields = { 64, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 48 } }, 56, 40, 1437, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 88, 40, 1337, { .fields = { 72, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, 104, 40, 1337, { .fields = { 88, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, 112, 32, 1325, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 32, 8 } }, -16, 32, 1387, { .fields = { 96, 16 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 8 } }, 16, 32, 1387, { .fields = { 96, 64 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 8 } }, 56, 32, 1442, { .fields = { 112, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, 72, 16, 1337, { .fields = { 8, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 96, 24, 1325, { .fields = { 8, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 88 } }, -32, -48, 2000, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 112 } }, -64, -64, 1587, { .fields = { 96, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, -72, -80, 1562, { .fields = { 16, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, -64, -56, 1562, { .fields = { 80, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 56 } }, -72, 8, 1562, { .fields = { 0, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -16, -72, 1750, { .fields = { 32, 136 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 24, 8 } }, 0, -48, 1750, { .fields = { 120, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -8, -40, 1750, { .fields = { 16, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 16, -32, 1750, { .fields = { 48, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 48 } }, -16, 8, 1750, { .fields = { 104, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 48 } }, 16, 8, 1750, { .fields = { 120, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -24, 40, 1750, { .fields = { 56, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 72, -104, 1325, { .fields = { 40, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, 88, -112, 1325, { .fields = { 0, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 120 } }, 88, -88, 1325, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 64, 32 } }, 64, 32, 1325, { .fields = { 112, 104 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 32, 16 } }, 88, -72, 1500, { .fields = { 104, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 80 } }, 96, -56, 1500, { .fields = { 40, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 40 } }, 80, 24, 1500, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 128, 96, 648, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 136, 96, 655, { .fields = { 80, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 144, 96, 661, { .fields = { 48, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 152, 96, 656, { .fields = { 64, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 80, 104, 366, { .fields = { 88, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 88, 104, 348, { .fields = { 80, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 96, 104, 612, { .fields = { 72, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 104, 104, 612, { .fields = { 48, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 112, 104, 612, { .fields = { 56, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 120, 104, 612, { .fields = { 80, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 128, 104, 612, { .fields = { 104, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 136, 104, 448, { .fields = { 88, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 144, 104, 612, { .fields = { 104, 104 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 8, 8 } }, 152, 104, 417, { .fields = { 120, 200 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 8, 8 } }, 80, 112, 356, { .fields = { 120, 184 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 8, 8 } }, 88, 112, 363, { .fields = { 112, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 96, 112, 569, { .fields = { 16, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 104, 112, 351, { .fields = { 24, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 112, 112, 359, { .fields = { 8, 160 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 8, 8 } }, 120, 112, 347, { .fields = { 120, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 128, 112, 345, { .fields = { 24, 160 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 8, 8 } }, 136, 112, 352, { .fields = { 112, 184 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 8, 8 } }, 144, 112, 569, { .fields = { 112, 176 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 8, 8 } }, 152, 112, 569, { .fields = { 120, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, -160, 80, 625, { .fields = { 72, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 32 } }, -152, 88, 625, { .fields = { 48, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, -128, 96, 625, { .fields = { 96, 232 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 16 } }, -96, 104, 625, { .fields = { 88, 136 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 16 } }, -48, 104, 625, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 16 } }, 0, 104, 625, { .fields = { 96, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, 40, 104, 625, { .fields = { 40, 240 } }, 128, 128, 128, 0 },
};

SpriteBatch D_acropolis_fountain_80182F64[15] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 5, 0, 0, { 2, 0 } },
    { 5, 3, 0, 0, { 7, 0 } },
    { 8, 8, 0, 0, { 6, 0 } },
    { 16, 3, 0, 0, { 8, 0 } },
    { 19, 4, 0, 0, { 0, 0 } },
    { 23, 19, 0, 0, { 9, 0 } },
    { 42, 1, 0, 0, { 5, 0 } },
    { 43, 1, 0, 0, { 10, 0 } },
    { 44, 3, 0, 0, { 1, 0 } },
    { 47, 7, 0, 0, { 11, 0 } },
    { 54, 4, 0, 0, { 4, 0 } },
    { 58, 3, 0, 0, { 12, 0 } },
    { 61, 31, 0, 0, { 3, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_acropolis_fountain_80182FDC[39] = {
    { 142, 0x3FC0, { .fields = { 40, 32 } }, -160, 88, 500, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -24, 80, 675, { .fields = { 40, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, -32, 48, 675, { .fields = { 80, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -40, 64, 625, { .fields = { 88, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -56, 72, 575, { .fields = { 112, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -72, 80, 550, { .fields = { 8, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -88, 80, 525, { .fields = { 24, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -104, 88, 500, { .fields = { 104, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -120, 88, 500, { .fields = { 56, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 16 } }, -72, 40, 900, { .fields = { 64, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, -80, 56, 700, { .fields = { 56, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 8 } }, -104, 64, 575, { .fields = { 40, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 72, 8 } }, -128, 72, 550, { .fields = { 40, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 8 } }, -152, 80, 500, { .fields = { 48, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 64 } }, -160, 32, 1375, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 72 } }, -136, 32, 1325, { .fields = { 32, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -64, 40, 1250, { .fields = { 72, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 88 } }, -88, 32, 1300, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 16 } }, -61, -120, 1087, { .fields = { 64, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 144 } }, -53, -104, 1087, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 24 } }, -77, 40, 1087, { .fields = { 56, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 80, 16 } }, -85, 64, 1125, { .fields = { 32, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 32 } }, -160, -120, 750, { .fields = { 80, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 24 } }, -160, -88, 750, { .fields = { 32, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 112 } }, -160, -64, 750, { .fields = { 112, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -120, -104, 1325, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 24 } }, -144, -96, 1325, { .fields = { 64, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 96 } }, -128, -72, 1325, { .fields = { 88, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 16 } }, -152, 24, 1325, { .fields = { 56, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 56 } }, 128, -16, 675, { .fields = { 0, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 48 } }, 128, -120, 675, { .fields = { 120, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 56 } }, 128, -72, 675, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 48 } }, 104, -120, 675, { .fields = { 104, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 64 } }, 104, -72, 675, { .fields = { 48, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 80, -104, 675, { .fields = { 0, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, 80, -56, 675, { .fields = { 112, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 64 } }, 56, -80, 675, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 40, -48, 675, { .fields = { 72, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 56 } }, 136, 40, 987, { .fields = { 24, 160 } }, 128, 128, 128, 0 },
};

SpriteBatch D_acropolis_fountain_801832E8[8] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 14, 0, 0, { 3, 0 } },
    { 14, 4, 0, 0, { 0, 0 } },
    { 18, 4, 0, 0, { 5, 0 } },
    { 22, 3, 0, 0, { 1, 0 } },
    { 25, 4, 0, 0, { 4, 0 } },
    { 29, 10, 0, 0, { 2, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_acropolis_fountain_80183328[8] = {
    { 143, 0x3FC0, { .fields = { 40, 80 } }, -112, 40, 675, { .fields = { 88, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, -72, 40, 700, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -64, 40, 725, { .fields = { 112, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -64, 80, 700, { .fields = { 80, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -56, 80, 700, { .fields = { 80, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -48, 88, 725, { .fields = { 104, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 16 } }, -120, 24, 700, { .fields = { 56, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 144 } }, -112, -120, 700, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
};

SpriteBatch D_acropolis_fountain_801833C8[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 8, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_acropolis_fountain_801833E0[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_acropolis_fountain_801833F0[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_acropolis_fountain_80183400[28] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -32, -88, 1750, { .fields = { 80, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -24, -88, 1750, { .fields = { 80, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -24, -16, 1750, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 16, -8, 1875, { .fields = { 112, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -32, 0, 1875, { .fields = { 80, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -24, -56, 1750, { .fields = { 88, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 16, -32, 1775, { .fields = { 80, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -40, -80, 1750, { .fields = { 112, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -32, -56, 1750, { .fields = { 80, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -24, -64, 1750, { .fields = { 80, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -24, -80, 1750, { .fields = { 80, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 88, 72, 500, { .fields = { 80, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -48, 96, 500, { .fields = { 80, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -40, 88, 500, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -16, 80, 500, { .fields = { 88, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -8, 72, 500, { .fields = { 64, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 24, 96, 500, { .fields = { 88, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 24, 80, 500, { .fields = { 80, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 40, 72, 500, { .fields = { 104, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 48, 80, 500, { .fields = { 88, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 88, 80, 500, { .fields = { 88, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 128, 80, 500, { .fields = { 56, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 136, 112, 500, { .fields = { 72, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 144, 64, 500, { .fields = { 72, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 128, 64, 500, { .fields = { 80, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 104, 56, 500, { .fields = { 80, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 72, 64, 500, { .fields = { 72, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 48, 72, 500, { .fields = { 72, 240 } }, 128, 128, 128, 0 },
};

SpriteBatch D_acropolis_fountain_80183630[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 11, 0, 0, { 1, 0 } },
    { 11, 17, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_acropolis_fountain_80183650[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_acropolis_fountain_80183660[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_acropolis_fountain_80183670[9] = {
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -160, 80, 812, { .fields = { 88, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -120, 80, 750, { .fields = { 88, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -80, 80, 750, { .fields = { 48, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -40, 80, 750, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 0, 80, 750, { .fields = { 88, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 40, 80, 750, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 80, 80, 750, { .fields = { 88, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 120, 80, 8125, { .fields = { 88, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -160, 72, 875, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
};

SpriteBatch D_acropolis_fountain_80183724[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 9, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_acropolis_fountain_8018373C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_acropolis_fountain_8018374C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView D_acropolis_fountain_8018375C[22] = {
    { { .empty = D_acropolis_fountain_8017FF4C }, D_acropolis_fountain_8017FF4C, NULL },
    { { .empty = D_acropolis_fountain_8017FF5C }, D_acropolis_fountain_8017FF5C, NULL },
    { { .elements = D_acropolis_fountain_8017FF6C }, D_acropolis_fountain_801806D8, NULL },
    { { .elements = D_acropolis_fountain_80180720 }, D_acropolis_fountain_80180DB0, NULL },
    { { .elements = D_acropolis_fountain_80180E28 }, D_acropolis_fountain_801813DC, NULL },
    { { .elements = D_acropolis_fountain_8018141C }, D_acropolis_fountain_8018155C, NULL },
    { { .elements = D_acropolis_fountain_80181574 }, D_acropolis_fountain_80181B00, NULL },
    { { .elements = D_acropolis_fountain_80181B48 }, D_acropolis_fountain_80182304, NULL },
    { { .empty = D_acropolis_fountain_80182394 }, D_acropolis_fountain_80182394, NULL },
    { { .empty = D_acropolis_fountain_801823A4 }, D_acropolis_fountain_801823A4, NULL },
    { { .elements = D_acropolis_fountain_801823B4 }, D_acropolis_fountain_801827EC, NULL },
    { { .elements = D_acropolis_fountain_80182834 }, D_acropolis_fountain_80182F64, NULL },
    { { .elements = D_acropolis_fountain_80182FDC }, D_acropolis_fountain_801832E8, NULL },
    { { .elements = D_acropolis_fountain_80183328 }, D_acropolis_fountain_801833C8, NULL },
    { { .elements = D_acropolis_fountain_80181574 }, D_acropolis_fountain_80181B00, NULL },
    { { .empty = D_acropolis_fountain_801833F0 }, D_acropolis_fountain_801833F0, NULL },
    { { .elements = D_acropolis_fountain_80183400 }, D_acropolis_fountain_80183630, NULL },
    { { .empty = D_acropolis_fountain_80183650 }, D_acropolis_fountain_80183650, NULL },
    { { .empty = D_acropolis_fountain_80183660 }, D_acropolis_fountain_80183660, NULL },
    { { .elements = D_acropolis_fountain_80183670 }, D_acropolis_fountain_80183724, NULL },
    { { .empty = D_acropolis_fountain_8018373C }, D_acropolis_fountain_8018373C, NULL },
    { { .empty = D_acropolis_fountain_8018374C }, D_acropolis_fountain_8018374C, NULL },
};

ViewCamera D_acropolis_fountain_80183864[21] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { 0, 0x6B6C, 1550 } }, 464 },
    { { { { 4075, 0, -413 }, { -8, 4095, -82 }, { 413, 83, 4074 } }, { 4500, 978, 2629 } }, 257 },
    { { { { 2002, 0, 3572 }, { 389, 4071, -218 }, { -3551, 446, 1990 } }, { -110, 1231, 5550 } }, 230 },
    { { { { 3982, 0, 958 }, { -16, 4095, 69 }, { -958, -71, 3981 } }, { -1500, 1380, 7850 } }, 225 },
    { { { { 1328, 0, -3874 }, { 188, 4091, 64 }, { 3869, -199, 1326 } }, { 1661, 1176, 4693 } }, 246 },
    { { { { 4065, 0, 496 }, { -4, 4095, 33 }, { -496, -33, 4065 } }, { -4642, 1201, 2854 } }, 240 },
    { { { { 4076, 0, 402 }, { -2, 4095, 29 }, { -402, -29, 4076 } }, { -3070, 932, 9060 } }, 246 },
    { { { { 636, 0, 4046 }, { 405, 4075, -63 }, { -4025, 410, 633 } }, { -6284, 1178, 7770 } }, 246 },
    { { { { 1820, 0, 3668 }, { 3342, 1688, -1658 }, { -1512, 3731, 750 } }, { 4570, 2369, 3789 } }, 282 },
    { { { { 4075, 0, -413 }, { -8, 4095, -82 }, { 413, 83, 4074 } }, { 4500, 978, 2629 } }, 257 },
    { { { { 2002, 0, 3572 }, { 389, 4071, -218 }, { -3551, 446, 1990 } }, { -109, 1230, 5549 } }, 230 },
    { { { { 3982, 0, 958 }, { -16, 4095, 69 }, { -958, -71, 3981 } }, { -1499, 1379, 7849 } }, 225 },
    { { { { 1328, 0, -3874 }, { 188, 4091, 64 }, { 3869, -199, 1326 } }, { 1661, 1176, 4693 } }, 246 },
    { { { { 4065, 0, 496 }, { -4, 4095, 33 }, { -496, -33, 4065 } }, { -4642, 1201, 2854 } }, 240 },
    { { { { 4076, 0, 402 }, { -2, 4095, 29 }, { -402, -29, 4076 } }, { -3070, 932, 9060 } }, 246 },
    { { { { 3966, 0, 1021 }, { 564, 3413, -2192 }, { -850, 2264, 3305 } }, { -1510, 4820, 6130 } }, 207 },
    { { { { 3677, 0, 1803 }, { 743, 3731, -1515 }, { -1643, 1688, 3350 } }, { -3139, 4280, 6307 } }, 221 },
    { { { { 3815, 0, 1489 }, { 981, 3080, -2514 }, { -1120, 2699, 2869 } }, { -920, 3010, 3600 } }, 230 },
    { { { { 1328, 0, -3874 }, { 188, 4091, 64 }, { 3869, -199, 1326 } }, { 1661, 1176, 4693 } }, 246 },
    { { { { 4006, 0, 850 }, { -171, 4011, 810 }, { -832, -828, 3924 } }, { -830, 950, 4980 } }, 221 },
    { { { { 3977, 0, 977 }, { 620, 3162, -2527 }, { -754, 2602, 3071 } }, { -4174, 1018, 7732 } }, 246 },
};

WorldCollisionFootstepSounds D_acropolis_fountain_80183B58 = {
    0x10000011,
    0x10000013,
    0x10000011,
};

WorldCollisionFootstepSounds D_acropolis_fountain_80183B64 = {
    0x10000039,
    0x1000003B,
    0x10000039,
};

WorldCollisionSurfaceProperties D_acropolis_fountain_80183B70[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_acropolis_fountain_80183B78[1] = { 0 };

WorldCollisionSurfaceProperties D_acropolis_fountain_80183B80[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_acropolis_fountain_80183B58 },
};

WorldCollisionSurfaceProperties D_acropolis_fountain_80183B88[1] = {
    { 1, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_acropolis_fountain_80183B64 },
};

WorldCollisionSurfaceProperties* D_acropolis_fountain_80183B90[8] = {
    D_acropolis_fountain_80183B70,
    D_acropolis_fountain_80183B70,
    D_acropolis_fountain_80183B78,
    D_acropolis_fountain_80183B70,
    D_acropolis_fountain_80183B80,
    D_acropolis_fountain_80183B88,
    D_acropolis_fountain_80183B70,
    D_acropolis_fountain_80183B70,
};

u8 D_acropolis_fountain_80183BB0 = 0;

u8 D_acropolis_fountain_80183BB1 = 0;

u16 D_acropolis_fountain_80183BB2 = 8192;

Task* D_acropolis_fountain_80183BB4 = NULL;

static void func_acropolis_fountain_8017DAA4(Task* arg0);
static void func_acropolis_fountain_8017DB00(Task* arg0);
static void func_acropolis_fountain_8017DB54(Task* arg0);
static void func_acropolis_fountain_8017DBAC(Task* arg0);
static void func_acropolis_fountain_8017DC00(Task* arg0);
static void func_acropolis_fountain_8017DC6C(Task* arg0);

void func_acropolis_fountain_8017DA1C(void)
{
    Gp_UnlinkObj4A(0, &D_acropolis_fountain_8017F9C0[5]);
    D_acropolis_fountain_8017E7A4.coord = &gGfxViewCoord;
    Gp_LinkObj4A(0, &D_acropolis_fountain_8017E7A4);
    D_acropolis_fountain_8017E7A4.flags |= WORLD_COLLISION_TRIGGER_ENABLED;
}

void func_acropolis_fountain_8017DA78(s32 unused0, s32 unused1)
{
    Task_Spawn(2, 0xE, 0, 0);
}

static void func_acropolis_fountain_8017DAA4(Task* arg0)
{
    ActorTransform msg;
    Task*          slot;

    slot       = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    msg.rot.vx = 0;
    msg.rot.vy = 0x800;
    msg.rot.vz = 0;
    TASK_MESSAGE_DISPATCH_POINTER(slot, 0x3EE, &msg, 0);
    arg0->state = arg0->state + 1;
}

static void func_acropolis_fountain_8017DB00(Task* arg0)
{
    if (taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_IS_SCRIPTED_MOTION_PENDING, 0, 0) == 0) {
        arg0->state = (s32)(arg0->state + 1);
    }
}

static void func_acropolis_fountain_8017DB54(Task* arg0)
{
    GameActorStairClimb climb;
    Task*               slot;

    slot            = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    climb.descend   = 0;
    climb.stepCount = 1;
    TASK_MESSAGE_DISPATCH_POINTER(slot, GAME_ACTOR_MESSAGE_CLIMB_STAIRS, &climb, 0);
    arg0->state = arg0->state + 1;
}

static void func_acropolis_fountain_8017DBAC(Task* arg0)
{
    if (taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER), GAME_ACTOR_MESSAGE_IS_SCRIPTED_MOTION_PENDING, 0, 0) == 0) {
        arg0->state = (s32)(arg0->state + 1);
    }
}

static void func_acropolis_fountain_8017DC00(Task* arg0)
{
    ActorTransform msg;
    Task*          slot;

    gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    slot       = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    msg.pos.vx = 0xA27;
    msg.pos.vy = -0xC8;
    msg.pos.vz = -0x17A6;
    TASK_MESSAGE_DISPATCH_POINTER(slot, 0x3F2, &msg, 0);
    arg0->state = arg0->state + 1;
}

static void func_acropolis_fountain_8017DC6C(Task* arg0)
{
    Task* temp_v0;

    temp_v0 = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    if (taskMessageDispatch(temp_v0, GAME_ACTOR_MESSAGE_IS_SCRIPTED_MOTION_PENDING, 0, 0) == 0) {
        taskMessageDispatch(temp_v0, GAME_ACTOR_MESSAGE_END_SCRIPTED, 0, 0);
        taskKill(arg0);
    }
}

/// Six-state dispatcher of the fountain cutscene task; the handler table is
/// built on the stack from the overlay's rodata block.
void func_acropolis_fountain_8017DCD4(Task* arg0)
{
    TaskFunc states[6] = {
        func_acropolis_fountain_8017DAA4,
        func_acropolis_fountain_8017DB00,
        func_acropolis_fountain_8017DB54,
        func_acropolis_fountain_8017DBAC,
        func_acropolis_fountain_8017DC00,
        func_acropolis_fountain_8017DC6C,
    };

    states[arg0->state](arg0);
}

/// Draws the fountain's water-spray sprite for the current frame. The task's
/// coordinate is refreshed and projected through `GsWSMATRIX` into a
/// `RoomGlowSpriteScratch` block; the resulting screen point becomes the centre of a
/// semi-transparent `POLY_FT4` (tpage 0x2B, clut 0x4382, the 0x28x0x27 cell at
/// u 0x50) whose half-extent is `0x4E00 / otz`, so the spray shrinks with
/// distance and is dropped entirely inside `otz` 0x11. The grey level
/// alternates between 0x40 and 0x50 with the frame counter's low bit, which
/// makes the spray flicker. Only the eight camera views in the `0x1040C0` mask
/// see the fountain, and the whole draw is skipped once `gRoomEffectState->effectControl`
/// reaches 4 (the room is fading out).
void func_acropolis_fountain_8017DD44(Task* task)
{
    RoomGlowSpriteScratch* blk;
    GfxCoord*              coord;
    POLY_FT4*              prim;
    s16                    x;
    s16                    y;
    s32                    level;

    coord = task->extra.coordBody->coord;
    if (gRoomEffectState->effectControl < ROOM_EFFECT_CONTROL_CANCEL_MIN && ((0x1040C0 >> (gGameSession->location.loc.view - 1)) & 1)) {
        actorRenderComposeCoord(coord);
        blk              = SCRATCH_STACK_RESERVE_BLOCK(RoomGlowSpriteScratch);
        blk->worldPos.vx = coord->workm.t[0];
        blk->worldPos.vy = coord->workm.t[1];
        blk->worldPos.vz = coord->workm.t[2];
        gte_SetTransMatrix(&GsWSMATRIX);
        gte_SetRotMatrix(&GsWSMATRIX);
        gte_ldv0(&blk->worldPos);
        gte_rtps();
        prim           = gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2C);
        gte_stsxy(&blk->screenPos);
        gte_stszotz(&blk->otz);
        if (blk->otz >= 0x11) {
            level           = (((u8)gDisplayState.animFrame & 1) << 4) + 0x40;
            prim->tpage     = 0x2B;
            prim->clut      = 0x4382;
            prim->u0        = 0x50;
            prim->v0        = 0;
            prim->u1        = 0x77;
            prim->v1        = 0;
            prim->u2        = 0x50;
            prim->v2        = 0x27;
            prim->u3        = 0x77;
            prim->v3        = 0x27;
            prim->r0        = level;
            prim->g0        = level;
            prim->b0        = level;
            prim->code     |= 2;
            blk->halfExtent = 0x4E00 / blk->otz;
            x               = blk->screenPos.vx - blk->halfExtent;
            prim->x2        = x;
            prim->x0        = x;
            x               = blk->screenPos.vx + blk->halfExtent;
            prim->x3        = x;
            prim->x1        = x;
            y               = blk->screenPos.vy - blk->halfExtent;
            prim->y1        = y;
            prim->y0        = y;
            y               = blk->screenPos.vy + blk->halfExtent;
            prim->y3        = y;
            prim->y2        = y;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)blk->otz
                                                               << gDisplayState.otDepthShift) >>
                                                              2) &
                                                             GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
        }
        SCRATCH_STACK_RELEASE_BLOCK(RoomGlowSpriteScratch);
    }
}

void func_acropolis_fountain_8017E014(Task* task)
{
    EffectWork* work;
    GfxCoord*   coord;
    s32         view;
    s32         one;
    s32         mask;
    s32         bit;
    s16         id;

    // This task draws nothing itself, so its effect work is free storage:
    // `scale` latches the view index the movie task was last spawned under,
    // and a camera cut replaces that task.
    coord = task->extra.coordBody->coord;
    work  = task->spawnArg2.pointer;
    view  = Gp_GetViewIndex();
    switch (task->state) {
        case 0:
            Gp_SpawnEff(EFFECT_ACROPOLIS_FOUNTAIN_SPRAY, coord, 0, &D_acropolis_fountain_8017E7F0);
            work->scale = view & 0xFF;
            task->state = 1;
            /* fallthrough */
        case 1:
            mask = 0x100FE;
            bit  = 1 << (work->scale - 1);
            if (bit & mask) {
                taskSpawnFromTable(D_acropolis_fountain_8017E7FC, 0, 0, 0);
            }
            task->state = 2;
            break;
        case 2:
            one = 1;
            id  = work->scale;
            bit = one << (id - 1);
            if (id != (view & 0xFF)) {
                if (bit & 0x100FE) {
                    taskSpawnFromTable(D_acropolis_fountain_8017E7FC, 1, 0, 0);
                }
                work->scale = (u8)view;
                task->state = 1;
            }
            break;
    }
}

static void func_acropolis_fountain_8017E15C(Task* task, s32 view)
{
    _AcropolisFountainWaterLoop* waterLoop;
    CdCmdQueue*                  queue;
    u16                          frame;

    queue     = &gCdCmdQueue;
    waterLoop = (_AcropolisFountainWaterLoop*)task->work;
    switch (waterLoop->state) {
        case ACROPOLIS_FOUNTAIN_WATER_LOOP_WAIT:
            // Outside the window, fade a running loop out. Inside it, command
            // the loop the first time, and again only after that fade. A new
            // task has not seen the window, so a camera cut re-pans a loop
            // the previous task left running.
            frame = queue->movieFrame;
            if (frame >= ACROPOLIS_FOUNTAIN_WATER_LOOP_FRAME_FADE) {
                if (D_acropolis_fountain_8017E7F8 != 0) {
                    SndEvt_EnqueueType7(SOUND_ACROPOLIS_FOUNTAIN_WATER_LOOP, ACROPOLIS_FOUNTAIN_WATER_LOOP_FADE_TICKS);
                    D_acropolis_fountain_8017E7F8 = 0;
                }
            } else if ((u16)(frame - ACROPOLIS_FOUNTAIN_WATER_LOOP_FRAME_FIRST) < ACROPOLIS_FOUNTAIN_WATER_LOOP_WINDOW) {
                if (waterLoop->seenWindow == 0 || D_acropolis_fountain_8017E7F8 == 0) {
                    waterLoop->seenWindow = 1;
                    waterLoop->state      = waterLoop->state + 1;
                }
            }
            break;

        case ACROPOLIS_FOUNTAIN_WATER_LOOP_DEFER:
            // The command waits one tick after the window accepts it.
            waterLoop->state = ACROPOLIS_FOUNTAIN_WATER_LOOP_APPLY;
            break;

        case ACROPOLIS_FOUNTAIN_WATER_LOOP_APPLY:
            // Indices 2..8 re-pan a running loop or start one, at that index's
            // pan and depth. The loop is then marked running for every index.
            switch ((u16)view) {
                case 2:
                case 3:
                    if (D_acropolis_fountain_8017E7F8 != 0) {
                        SndEvt_EnqueueTypeA(SOUND_ACROPOLIS_FOUNTAIN_WATER_LOOP, 7, 2);
                    } else {
                        sndEvtRequestScriptStart(SOUND_ACROPOLIS_FOUNTAIN_WATER_LOOP, 7, 2);
                    }
                    break;
                case 4:
                    if (D_acropolis_fountain_8017E7F8 != 0) {
                        SndEvt_EnqueueTypeA(SOUND_ACROPOLIS_FOUNTAIN_WATER_LOOP, 0, 0);
                    } else {
                        sndEvtRequestScriptStart(SOUND_ACROPOLIS_FOUNTAIN_WATER_LOOP, 0, 0);
                    }
                    break;
                case 5:
                    if (D_acropolis_fountain_8017E7F8 != 0) {
                        SndEvt_EnqueueTypeA(SOUND_ACROPOLIS_FOUNTAIN_WATER_LOOP, -7, 2);
                    } else {
                        sndEvtRequestScriptStart(SOUND_ACROPOLIS_FOUNTAIN_WATER_LOOP, -7, 2);
                    }
                    break;
                case 6:
                    if (D_acropolis_fountain_8017E7F8 != 0) {
                        SndEvt_EnqueueTypeA(SOUND_ACROPOLIS_FOUNTAIN_WATER_LOOP, -7, 0);
                    } else {
                        sndEvtRequestScriptStart(SOUND_ACROPOLIS_FOUNTAIN_WATER_LOOP, -7, 0);
                    }
                    break;
                case 7:
                    if (D_acropolis_fountain_8017E7F8 != 0) {
                        SndEvt_EnqueueTypeA(SOUND_ACROPOLIS_FOUNTAIN_WATER_LOOP, 0, 2);
                    } else {
                        sndEvtRequestScriptStart(SOUND_ACROPOLIS_FOUNTAIN_WATER_LOOP, 0, 2);
                    }
                    break;
                case 8:
                    if (D_acropolis_fountain_8017E7F8 != 0) {
                        SndEvt_EnqueueTypeA(SOUND_ACROPOLIS_FOUNTAIN_WATER_LOOP, 7, 3);
                    } else {
                        sndEvtRequestScriptStart(SOUND_ACROPOLIS_FOUNTAIN_WATER_LOOP, 7, 3);
                    }
                    break;
            }
            D_acropolis_fountain_8017E7F8 = 1;
            waterLoop->state              = ACROPOLIS_FOUNTAIN_WATER_LOOP_WAIT;
            break;
    }
}

void func_acropolis_fountain_8017E3D4(Task* task)
{
    CdCmdQueue* queue;
    CdCmdQueue* queue2;
    StreamSlot* textureStream;
    StreamSlot* loopStream;
    SPRT*       p;
    DR_TPAGE*   dr;
    GameLoc     key;
    GameLoc     key2;
    s16         view;
    s32         ot;
    u16         count;
    u32         tpage;

    D_acropolis_fountain_80183BB4 = task;
    queue                         = &gCdCmdQueue;
    if (gGameSession->location.loc.room != 1) {
        return;
    }
    switch (task->state) {
        case 0:
            task->work = memCalloc(sizeof(_AcropolisFountainWaterLoop), 0);
            if (task->work == NULL) {
                taskKill(task);
                return;
            }
            memFillBytes(task->work, 0, sizeof(_AcropolisFountainWaterLoop));
            task->state = task->state + 1;
            break;

        case 1:
            view = Gp_FindViewIndex(gGameSession->location.loc.view);
            func_acropolis_fountain_8017E15C(task, (u16)view);
            switch ((u16)view) {
                case 3:
                case 5:
                    if (queue->suppressMoviePresentation != 0) {
                        return;
                    }
                    gGameSession->field_4E = 1;
                    key                    = gGameSession->location;
                    key.loc.view           = view;
                    textureStream          = Stream_GetSlot(Stream_FindSlotByKey((u8*)&key) & 0xFFFF);

                    p              = gGpuPrimCursor;
                    gGpuPrimCursor = p + 1;
                    setlen(p, 4);
                    setcode(p, 0x65);
                    p->u0 = 0;
                    p->v0 = 0;
                    if ((u16)view == 3) {
                        ot    = 0x23;
                        p->x0 = 0x70;
                        p->y0 = 8;
                    } else if ((u16)view == 5) {
                        ot    = 0x1D;
                        p->x0 = -0xA0;
                        p->y0 = 0x2A;
                    }
                    dr   = gGpuPrimCursor;
                    p->w = textureStream->data.movie.width;
                    p->h = textureStream->data.movie.height;
                    addPrim(&gGpuCurrentOt[ot], p);

                    gGpuPrimCursor = dr + 1;
                    setlen(dr, 1);
                    tpage       = (u32)(textureStream->data.movie.vramY & 0x100) >> 4;
                    dr->code[0] = tpage | (((u32)(textureStream->data.movie.vramX & 0x3FF) >> 6) | 0x100) |
                                  ((textureStream->data.movie.vramY & 0x200) * 4) | 0xE1000000;
                    addPrim(&gGpuCurrentOt[ot], dr);
                    break;

                case 2:
                case 8:
                    if (gDisplayState.animFrame & 1) {
                        queue2             = &gCdCmdQueue;
                        key2               = gGameSession->location;
                        key2.loc.view      = Gp_FindViewIndex(4);
                        loopStream         = Stream_GetSlot(Stream_FindSlot((u8*)&key2, 0, 1) & 0xFFFF);
                        count              = queue2->movieFrame + 1;
                        queue2->movieFrame = count;
                        if (count >= loopStream->data.movie.frameLimit - 0xA) {
                            queue2->movieFrame = 1;
                        }
                    }
                    break;
            }
            break;
    }
}

void func_acropolis_fountain_8017E72C(Task* arg0)
{
    taskKill(D_acropolis_fountain_80183BB4);
    taskKill(arg0);
}
