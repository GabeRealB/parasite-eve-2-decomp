#include "rooms/dryfield_night_motel_balcony.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>
#include <psyq/memory.h>

#include "gte.h"
#include "common.h"

#include "dryfield_night_motel_balcony_private.h"

#include "actors/task_tables.h"

#include "gameplay/display.h"
#include "gameplay/actor_render.h"
#include "gameplay/area.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/effects.h"
#include "gameplay/light.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"
#include "gameplay/world_collision.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gameflag.h"
#include "main/random.h"
#include "main/gfx.h"
#include "main/gfx_types.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "mapui/map_dryfield_full.h"

#include "overlay.h"

#include "rooms/room_common.h"
#include "../../shared/glow_draw.h"

extern SVECTOR D_dryfield_night_motel_balcony_80182CA0;
extern SVECTOR D_dryfield_night_motel_balcony_80182CF0;
extern SVECTOR D_dryfield_night_motel_balcony_80182D00;
extern SVECTOR D_dryfield_night_motel_balcony_80182D08;
extern SVECTOR D_dryfield_night_motel_balcony_80182D10;
extern SVECTOR D_dryfield_night_motel_balcony_80182D18;
extern SVECTOR D_dryfield_night_motel_balcony_80182D28;
extern SVECTOR D_dryfield_night_motel_balcony_80182D30;
extern SVECTOR D_dryfield_night_motel_balcony_80182D38;
extern s32     D_dryfield_night_motel_balcony_80182D40[2][20];
extern SVECTOR D_dryfield_night_motel_balcony_80182D20;

/// A horizontal row of eight square texture frames for the room's tumbling debris.
///
/// Frames start at U=0 and advance by `frameSize` texels. Their inclusive UV
/// span, `frameSize - 1`, also scales the projected billboard. Textures are
/// 4-bit, on VRAM page Y=0, with additive semitransparency when tinted.
/// The room's three rows are selected by the initialized debris task's
/// `spawnArg1.value` (0..2), which also selects its 16-colour palette.
typedef struct {
    u16 vramX;     // Texture-page X in 16-bit VRAM words, aligned to 64 words
    s16 frameSize; // Square frame side and horizontal frame step in texels (16 or 32)
    u8  v;         // Top texture row within the page, in texels (0..255)
    u8  field_5;   // Unread byte, zero in all three rows; role unproven
} _DryfieldNightMotelBalconyDebrisTextureRow;
STATIC_ASSERT_SIZEOF(_DryfieldNightMotelBalconyDebrisTextureRow, 6);

/// VRAM position of the first 16-colour CLUT in an animation row.
///
/// `x` and `y` are unencoded coordinates in the units `getClut` takes.
/// Each later frame starts 16 pixels further along `x`, the width of one
/// 4-bit CLUT, and the drawer packs that position with `getClut`.
typedef struct {
    s16 x; // First frame's palette X in VRAM pixels
    u16 y; // Palette Y in VRAM scanlines
} _ClutOrigin;
STATIC_ASSERT_SIZEOF(_ClutOrigin, 4);

extern _ClutOrigin D_dryfield_night_motel_balcony_80182DF4[];

static void func_dryfield_night_motel_balcony_8017FF78(Task* task, u8* color, s32 arg);
static void func_dryfield_night_motel_balcony_80180C60(Task* task, u8* color, s32 unused);
static void func_dryfield_night_motel_balcony_801819E0(Task* task, s32 arg);
static void func_dryfield_night_motel_balcony_8018221C(Task* task, u8* color, s16 tick);

extern WorldCollisionGrid         D_dryfield_night_motel_balcony_80183750[1];
extern WorldCollisionGrid         D_dryfield_night_motel_balcony_80183FE0[1];
extern WorldCollisionOccluder     D_dryfield_night_motel_balcony_8018EF70[2];
extern WorldCollisionTrigger      D_dryfield_night_motel_balcony_8018E2FC[8];
extern WorldCollisionTrigger      D_dryfield_night_motel_balcony_8018E55C[10];
extern WorldCollisionTrigger      D_dryfield_night_motel_balcony_8018E854[6];
extern WorldCollisionTrigger      D_dryfield_night_motel_balcony_8018EAFC[4];
extern WorldCollisionTrigger      D_dryfield_night_motel_balcony_8018EC2C[4];
extern WorldCollisionTrigger      D_dryfield_night_motel_balcony_8018ED5C[7];
extern WorldCoordRoomAmbientEntry D_dryfield_night_motel_balcony_8018EFE8[40];
extern WorldCoordRoomAmbientEntry D_dryfield_night_motel_balcony_8018F128[40];
extern WorldCoordRoomLights       D_dryfield_night_motel_balcony_8018DA8C[1];
extern WorldCoordRoomLights       D_dryfield_night_motel_balcony_8018E2E4[1];

extern WorldCollisionSurfaceProperties D_dryfield_night_motel_balcony_8018F28C[1];
extern WorldCollisionSurfaceProperties D_dryfield_night_motel_balcony_8018F294[1];
extern WorldCollisionSurfaceProperties D_dryfield_night_motel_balcony_8018F29C[1];
extern WorldCollisionSurfaceProperties D_dryfield_night_motel_balcony_8018F2A4[1];

SVECTOR D_dryfield_night_motel_balcony_80182C98 = { -160, -2800, 8790, 0 };

SVECTOR D_dryfield_night_motel_balcony_80182CA0 = { -300, -3190, -4610, 0 };

SVECTOR D_dryfield_night_motel_balcony_80182CA8[9] = {
    { -300, -3250, -4100, 0 },
    { -4140, -1700, 0x2AB2, 0 },
    { -6910, -1700, 0x299A, 0 },
    { -6910, -1700, 6660, 0 },
    { -6910, -1700, -3150, 0 },
    { -3150, -4850, 0x2AA8, 0 },
    { -6930, -4850, 5150, 0 },
    { -6930, -4850, 3870, 0 },
    { -6930, -4850, -3150, 0 },
};

SVECTOR D_dryfield_night_motel_balcony_80182CF0 = { -0x312E, -5670, 490, 0 };

SVECTOR D_dryfield_night_motel_balcony_80182CF8 = { -0x2F3A, -5320, 490, 0 };

SVECTOR D_dryfield_night_motel_balcony_80182D00 = { -6400, -4608, 0x2AA8, 0 };

SVECTOR D_dryfield_night_motel_balcony_80182D08 = { -2560, 0, -6144, 0 };

SVECTOR D_dryfield_night_motel_balcony_80182D10 = { -4608, 0, -2940, 0 };

SVECTOR D_dryfield_night_motel_balcony_80182D18 = { -5110, -3250, -4100, 0 };

SVECTOR D_dryfield_night_motel_balcony_80182D20 = { 0, -576, 1600, 0 };

SVECTOR D_dryfield_night_motel_balcony_80182D28 = { -6930, -3826, -3150, 0 };

SVECTOR D_dryfield_night_motel_balcony_80182D30 = { -3840, -1024, 1536, 0 };

SVECTOR D_dryfield_night_motel_balcony_80182D38 = { -6930, -4096, -1230, 0 };

s32 D_dryfield_night_motel_balcony_80182D40[2][20] = {
    { 0x80000, 0, 0x80000, 0, 0x20040000, 0, 0xE083E70, 512, 0x20056000, 0x40000, 0x40081000, 0x40080000, 0x40080000, 0, -0x312F6190, 0x4CC04668, 0x4CC04668, 0x9024640, 132, 0 },
    { 0, 0, 128, 128, 129, 128, 24, 0, 128, 0, 0, 0, 129, 128, 25, 105, 109, 72, 2, 0 },
};

_DryfieldNightMotelBalconyDebrisTextureRow D_dryfield_night_motel_balcony_80182DE0[3] = {
    { 704, 16, 240, 0 },
    { 768, 32, 0, 0 },
    { 768, 32, 32, 0 },
};

_ClutOrigin D_dryfield_night_motel_balcony_80182DF4[3] = {
    { 64, 271 },
    { 0, 263 },
    { 0, 264 },
};

WorldCoordRoomLighting D_dryfield_night_motel_balcony_80182E00[3] = {
    { D_dryfield_night_motel_balcony_8018DA8C, D_dryfield_night_motel_balcony_8018EFE8 },
    { D_dryfield_night_motel_balcony_8018E2E4, D_dryfield_night_motel_balcony_8018F128 },
    { D_dryfield_night_motel_balcony_8018DA8C, D_dryfield_night_motel_balcony_8018EFE8 },
};

WorldCollisionRoomResources D_dryfield_night_motel_balcony_80182E18[3] = {
    { D_dryfield_night_motel_balcony_80183750, D_dryfield_night_motel_balcony_8018E2FC, D_dryfield_night_motel_balcony_8018EAFC, D_dryfield_night_motel_balcony_8018EF70 },
    { D_dryfield_night_motel_balcony_80183750, D_dryfield_night_motel_balcony_8018E55C, D_dryfield_night_motel_balcony_8018EC2C, D_dryfield_night_motel_balcony_8018EF70 },
    { D_dryfield_night_motel_balcony_80183FE0, D_dryfield_night_motel_balcony_8018E854, D_dryfield_night_motel_balcony_8018ED5C, D_dryfield_night_motel_balcony_8018EF70 },
};

u8 D_dryfield_night_motel_balcony_80182E48[40] = {
    1,
    7,
    8,
    25,
    26,
    10,
    2,
    3,
    9,
    6,
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
    23,
    24,
    25,
    26,
    27,
    28,
    29,
    30,
    31,
    32,
    33,
    34,
    35,
    36,
    37,
    38,
    39,
    0,
};

u8 D_dryfield_night_motel_balcony_80182E70[40] = {
    1,
    33,
    34,
    31,
    32,
    35,
    2,
    3,
    9,
    6,
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
    23,
    24,
    25,
    26,
    27,
    28,
    29,
    30,
    31,
    32,
    33,
    34,
    35,
    36,
    37,
    38,
    39,
    0,
};

u8* D_dryfield_night_motel_balcony_80182E98[3] = {
    gViewIdentityMap,
    D_dryfield_night_motel_balcony_80182E48,
    D_dryfield_night_motel_balcony_80182E70,
};

ViewCount D_dryfield_night_motel_balcony_80182EA4[3] = { 39, 39, 39 };

DirectionWarpEntry D_dryfield_night_motel_balcony_80182EAC[5] = {
    { { { .word = 0 }, -0x315E, -3198, 1396 }, { 0, 0, 0, 0 }, { { .word = 0 }, -0x315E, -3198, 1396 }, { 0, 0, 0, 0 }, 0x531D0006, 0x531D0005, 0x531D0009, 2, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
    { { { .word = 1024 }, -6572, -3200, -2869 }, { 0, 0, 0, 0 }, { { .word = 1024 }, -6572, -3200, -2869 }, { 0, 0, 0, 0 }, 0x531D0002, 0x531D0001, 0x531D0009, 6, DIRECTION_WARP_FLAG_NONE, 464 },
    { { { .word = 1024 }, -6669, -3200, 4477 }, { 0, 0, 0, 0 }, { { .word = 1024 }, -6669, -3200, 4477 }, { 0, 0, 0, 0 }, 0x531D0002, 0x531D0001, 0x531D0009, 5, DIRECTION_WARP_FLAG_NONE, 465 },
    { { { .word = 2048 }, -1990, -3200, 0x29A8 }, { 0, 0, 0, 0 }, { { .word = 2048 }, -1990, -3200, 0x29A8 }, { 0, 0, 0, 0 }, 0x531D0002, 0x531D0001, 0x531D0009, 4, DIRECTION_WARP_FLAG_NONE, 466 },
    { { { .word = 1024 }, -6669, -3200, 4477 }, { 0, 0, 0, 0 }, { { .word = 1024 }, -6669, -3200, 4477 }, { 0, 0, 0, 0 }, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, 5, DIRECTION_WARP_FLAG_NONE, 465 },
};

static SVECTOR _gDryfieldNightMotelBalconyCollision06190Normals[7] = {
#include "assets/dryfield_night_motel_balcony_collision_06190_normals.inc"
};

static SVECTOR _gDryfieldNightMotelBalconyCollision06190Verts[69] = {
#include "assets/dryfield_night_motel_balcony_collision_06190_verts.inc"
};

static WorldCollisionGridFace _gDryfieldNightMotelBalconyCollision06190Faces[32] = {
#include "assets/dryfield_night_motel_balcony_collision_06190_faces.inc"
};

static s16 _gDryfieldNightMotelBalconyCollision06190Cells[342] = {
#include "assets/dryfield_night_motel_balcony_collision_06190_cells.inc"
};

#define GRID_CELL(i) (&_gDryfieldNightMotelBalconyCollision06190Cells[i])
static s16* _gDryfieldNightMotelBalconyCollision06190Table[64] = {
#include "assets/dryfield_night_motel_balcony_collision_06190_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid D_dryfield_night_motel_balcony_80183750[1] = {
    { NULL, _gDryfieldNightMotelBalconyCollision06190Normals, _gDryfieldNightMotelBalconyCollision06190Verts, _gDryfieldNightMotelBalconyCollision06190Faces, _gDryfieldNightMotelBalconyCollision06190Table, 0x32BE, 0x341C, 8, 8, 4000, 32 },
};

static SVECTOR _gDryfieldNightMotelBalconyCollision06A20Normals[11] = {
#include "assets/dryfield_night_motel_balcony_collision_06A20_normals.inc"
};

static SVECTOR _gDryfieldNightMotelBalconyCollision06A20Verts[81] = {
#include "assets/dryfield_night_motel_balcony_collision_06A20_verts.inc"
};

static WorldCollisionGridFace _gDryfieldNightMotelBalconyCollision06A20Faces[37] = {
#include "assets/dryfield_night_motel_balcony_collision_06A20_faces.inc"
};

static s16 _gDryfieldNightMotelBalconyCollision06A20Cells[360] = {
#include "assets/dryfield_night_motel_balcony_collision_06A20_cells.inc"
};

#define GRID_CELL(i) (&_gDryfieldNightMotelBalconyCollision06A20Cells[i])
static s16* _gDryfieldNightMotelBalconyCollision06A20Table[64] = {
#include "assets/dryfield_night_motel_balcony_collision_06A20_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid D_dryfield_night_motel_balcony_80183FE0[1] = {
    { NULL, _gDryfieldNightMotelBalconyCollision06A20Normals, _gDryfieldNightMotelBalconyCollision06A20Verts, _gDryfieldNightMotelBalconyCollision06A20Faces, _gDryfieldNightMotelBalconyCollision06A20Table, 0x32BE, 0x341C, 8, 8, 4000, 37 },
};

ViewCamera D_dryfield_night_motel_balcony_80184004[39] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { 4112, 0x88B8, -2748 } }, 380 },
    { { { { -559, 0, -4057 }, { -963, 3978, 132 }, { 3941, 972, -543 } }, { 0x4437, 4754, -1733 } }, 230 },
    { { { { 1192, 0, 3918 }, { 526, 4058, -160 }, { -3883, 550, 1181 } }, { 3977, 4660, -1257 } }, 230 },
    { { { { 3968, 0, -1013 }, { -310, 3898, -1217 }, { 964, 1256, 3777 } }, { 5913, 5512, -3603 } }, 230 },
    { { { { 3980, 0, -964 }, { 9, 4095, 40 }, { 964, -42, 3980 } }, { 6137, 4498, 2582 } }, 230 },
    { { { { 3960, 0, -1044 }, { 83, 4082, 317 }, { 1040, -328, 3947 } }, { 6205, 4066, 7032 } }, 230 },
    { { { { 257, 0, -4087 }, { -569, 4056, -35 }, { 4048, 570, 255 } }, { 0x3CEF, 4634, -1649 } }, 289 },
    { { { { 543, 0, -4059 }, { 386, 4077, 51 }, { 4041, -389, 540 } }, { 0x2D6F, 4037, -1767 } }, 289 },
    { { { { -1629, 0, 3757 }, { 362, 4076, 157 }, { -3740, 395, -1622 } }, { -2180, 4664, -0x298C } }, 230 },
    { { { { 3791, 0, -1548 }, { 139, 4079, 341 }, { 1542, -369, 3776 } }, { 6606, 4183, 7528 } }, 230 },
    { { { { 3674, 0, -1810 }, { -1285, 2883, -2609 }, { 1274, 2908, 2586 } }, { 5542, 9403, -3532 } }, 230 },
    { { { { 3475, 0, -2168 }, { -803, 3804, -1288 }, { 2013, 1518, 3227 } }, { 4659, 7233, 2080 } }, 230 },
    { { { { 2289, 0, -3396 }, { -1745, 3513, -1176 }, { 2913, 2104, 1963 } }, { 6624, 8520, 6055 } }, 230 },
    { { { { -3895, 0, -1265 }, { 436, 3844, -1344 }, { 1187, -1413, -3656 } }, { 6016, 3919, -0x2A09 } }, 230 },
    { { { { 3347, 0, -2360 }, { 1258, 3465, 1784 }, { 1997, -2183, 2831 } }, { 6606, 3324, 1736 } }, 230 },
    { { { { 3538, 0, -2062 }, { 822, 3756, 1411 }, { 1891, -1633, 3244 } }, { 6146, 3144, 8006 } }, 230 },
    { { { { -4087, 0, 270 }, { -89, 3867, -1344 }, { -255, -1347, -3859 } }, { 5661, 3313, -134 } }, 289 },
    { { { { -3824, 0, 1466 }, { -419, 3924, -1094 }, { -1405, -1172, -3664 } }, { 1367, 798, 24 } }, 230 },
    { { { { 3637, 0, -1883 }, { 328, 4033, 634 }, { 1854, -714, 3581 } }, { 6173, 491, 4712 } }, 230 },
    { { { { 3950, 0, -1083 }, { 204, 4022, 747 }, { 1064, -774, 3878 } }, { 6813, 3650, -7925 } }, 230 },
    { { { { 4092, 0, -163 }, { -89, 3427, -2240 }, { 137, 2241, 3425 } }, { 4200, 2740, 740 } }, 329 },
    { { { { 4006, 0, 850 }, { -182, 4001, 857 }, { -831, -876, 3913 } }, { 5560, 3880, -2000 } }, 329 },
    { { { { 4006, 0, 850 }, { -182, 4001, 857 }, { -831, -876, 3913 } }, { 6042, 3553, 3407 } }, 329 },
    { { { { 3135, 0, -2634 }, { 1166, 3672, 1388 }, { 2362, -1813, 2811 } }, { 6210, 6110, -4190 } }, 289 },
    { { { { 3968, 0, -1013 }, { -310, 3898, -1217 }, { 964, 1256, 3777 } }, { 5913, 5512, -3603 } }, 230 },
    { { { { 3980, 0, -964 }, { 9, 4095, 40 }, { 964, -42, 3980 } }, { 6137, 4498, 2582 } }, 230 },
    { { { { 3960, 0, -1044 }, { 83, 4082, 317 }, { 1040, -328, 3947 } }, { 6205, 4066, 7032 } }, 230 },
    { { { { 611, 0, -4050 }, { -580, 4053, -87 }, { 4008, 587, 605 } }, { 0x34E4, 5130, -1530 } }, 329 },
    { { { { -3824, 0, 1466 }, { -419, 3924, -1094 }, { -1405, -1172, -3664 } }, { 1367, 798, 24 } }, 230 },
    { { { { 3685, 0, 1788 }, { 67, 4093, -139 }, { -1786, 155, 3682 } }, { 4500, 4300, 2470 } }, 230 },
    { { { { 4021, 0, -776 }, { -94, 4065, -487 }, { 770, 496, 3991 } }, { 5620, 4940, -4510 } }, 230 },
    { { { { 4065, 0, 497 }, { -10, 4095, 82 }, { -497, -82, 4064 } }, { 5670, 4140, -930 } }, 230 },
    { { { { -559, 0, -4057 }, { -963, 3978, 132 }, { 3941, 972, -543 } }, { 0x4437, 4754, -1733 } }, 230 },
    { { { { 960, 0, 3981 }, { 404, 4074, -97 }, { -3961, 416, 955 } }, { 3520, 4170, -1056 } }, 230 },
    { { { { 3960, 0, -1044 }, { 83, 4082, 317 }, { 1040, -328, 3947 } }, { 6205, 4066, 7032 } }, 230 },
    { { { { 2854, 0, -2937 }, { -843, 3923, -819 }, { 2813, 1176, 2734 } }, { 6920, 4745, 740 } }, 230 },
    { { { { 4085, 0, 295 }, { 258, 1993, -3568 }, { -144, 3578, 1988 } }, { 2970, 0x2D32, 2685 } }, 289 },
    { { { { -3608, 0, -1938 }, { 627, 3875, -1168 }, { 1833, -1326, -3413 } }, { 6830, 3500, -9600 } }, 230 },
    { { { { -3526, 0, -2083 }, { 695, 3861, -1176 }, { 1963, -1366, -3324 } }, { 6640, 510, -9760 } }, 230 },
};

SpriteBatch D_dryfield_night_motel_balcony_80184580[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_night_motel_balcony_80184590[185] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 72, -120, 1690, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 96, -120, 2261, { .fields = { 96, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 104, -120, 2394, { .fields = { 120, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 72, -112, 1319, { .fields = { 104, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 80, -112, 1326, { .fields = { 112, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 88, -112, 1332, { .fields = { 104, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 96, -112, 1338, { .fields = { 72, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 104, -112, 1344, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 112, -112, 1351, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 120, -112, 1357, { .fields = { 96, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 128, -112, 1364, { .fields = { 104, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 136, -112, 1370, { .fields = { 88, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 144, -112, 1377, { .fields = { 72, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 152, -112, 1383, { .fields = { 88, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 72, -104, 1341, { .fields = { 80, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 80, -104, 1337, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 88, -104, 1353, { .fields = { 80, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 96, -104, 1360, { .fields = { 96, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 104, -104, 1366, { .fields = { 80, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 112, -104, 1373, { .fields = { 72, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 120, -104, 1379, { .fields = { 80, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 128, -104, 1386, { .fields = { 40, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 136, -104, 1393, { .fields = { 64, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 144, -104, 1398, { .fields = { 56, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 152, -104, 1405, { .fields = { 72, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 72, -96, 1352, { .fields = { 64, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 80, -96, 1357, { .fields = { 64, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 88, -96, 1364, { .fields = { 56, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 96, -96, 1371, { .fields = { 8, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 104, -96, 1377, { .fields = { 56, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 112, -96, 1384, { .fields = { 48, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 120, -96, 1391, { .fields = { 56, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 128, -96, 1397, { .fields = { 64, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 136, -96, 1404, { .fields = { 48, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 144, -96, 1411, { .fields = { 40, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 152, -96, 1418, { .fields = { 48, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 72, -88, 1359, { .fields = { 40, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 80, -88, 1369, { .fields = { 40, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 88, -88, 1376, { .fields = { 40, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 96, -88, 1382, { .fields = { 48, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 104, -88, 1389, { .fields = { 40, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 112, -88, 1396, { .fields = { 32, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 120, -88, 1402, { .fields = { 40, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 128, -88, 1409, { .fields = { 88, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 136, -88, 1416, { .fields = { 24, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 144, -88, 1423, { .fields = { 24, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 152, -88, 1430, { .fields = { 24, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 72, -80, 1371, { .fields = { 24, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 80, -80, 1380, { .fields = { 24, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 88, -80, 1387, { .fields = { 24, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 96, -80, 1394, { .fields = { 24, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 104, -80, 1400, { .fields = { 24, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 112, -80, 1407, { .fields = { 24, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 120, -80, 1410, { .fields = { 24, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 128, -80, 1416, { .fields = { 24, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 136, -80, 1428, { .fields = { 24, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 144, -80, 1435, { .fields = { 24, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 152, -80, 1443, { .fields = { 16, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 72, -72, 1388, { .fields = { 16, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 80, -72, 1392, { .fields = { 16, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 88, -72, 1399, { .fields = { 16, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 96, -72, 1405, { .fields = { 16, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 104, -72, 1412, { .fields = { 16, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 112, -72, 1419, { .fields = { 16, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 120, -72, 1432, { .fields = { 16, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 128, -72, 1443, { .fields = { 16, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 136, -72, 1450, { .fields = { 16, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 144, -72, 1456, { .fields = { 16, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 152, -72, 1455, { .fields = { 16, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 72, -64, 1399, { .fields = { 16, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 80, -64, 1404, { .fields = { 16, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 88, -64, 1410, { .fields = { 16, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 96, -64, 1417, { .fields = { 16, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 104, -64, 1424, { .fields = { 16, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 112, -64, 1431, { .fields = { 16, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 120, -64, 1446, { .fields = { 16, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 128, -64, 1456, { .fields = { 16, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 136, -64, 1463, { .fields = { 16, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 144, -64, 1468, { .fields = { 16, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 152, -64, 1468, { .fields = { 16, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 72, -56, 1406, { .fields = { 8, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 80, -56, 1415, { .fields = { 8, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 88, -56, 1422, { .fields = { 8, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 96, -56, 1429, { .fields = { 8, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 104, -56, 1437, { .fields = { 8, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 112, -56, 1444, { .fields = { 8, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 120, -56, 1461, { .fields = { 8, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 128, -56, 1468, { .fields = { 120, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 136, -56, 1476, { .fields = { 8, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 144, -56, 1478, { .fields = { 8, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 152, -56, 1481, { .fields = { 8, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 72, -48, 1415, { .fields = { 8, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 80, -48, 1427, { .fields = { 8, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 88, -48, 1435, { .fields = { 8, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 96, -48, 1442, { .fields = { 0, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 104, -48, 1449, { .fields = { 8, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 112, -48, 1456, { .fields = { 8, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 120, -48, 1483, { .fields = { 8, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 128, -48, 1491, { .fields = { 8, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 136, -48, 1499, { .fields = { 8, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 144, -48, 1487, { .fields = { 0, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 152, -48, 1494, { .fields = { 8, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 72, -40, 1433, { .fields = { 0, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 80, -40, 1440, { .fields = { 8, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 88, -40, 1447, { .fields = { 0, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 96, -40, 1454, { .fields = { 8, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 104, -40, 1462, { .fields = { 0, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 112, -40, 1469, { .fields = { 0, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 120, -40, 1496, { .fields = { 0, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 128, -40, 1504, { .fields = { 0, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 136, -40, 1512, { .fields = { 0, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 144, -40, 1495, { .fields = { 0, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 152, -40, 1508, { .fields = { 0, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 72, -32, 1380, { .fields = { 0, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 80, -32, 1452, { .fields = { 0, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 88, -32, 1460, { .fields = { 0, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 96, -32, 1467, { .fields = { 0, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 104, -32, 1475, { .fields = { 0, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 112, -32, 1482, { .fields = { 0, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 120, -32, 1451, { .fields = { 0, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 128, -32, 1427, { .fields = { 0, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 136, -32, 1456, { .fields = { 0, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 144, -32, 1513, { .fields = { 0, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 152, -32, 1521, { .fields = { 0, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 72, -24, 1392, { .fields = { 0, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 80, -24, 1465, { .fields = { 120, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 88, -24, 1472, { .fields = { 120, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 96, -24, 1480, { .fields = { 120, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 104, -24, 1488, { .fields = { 120, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 112, -24, 1495, { .fields = { 120, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 120, -24, 1502, { .fields = { 8, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 128, -24, 1418, { .fields = { 120, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 136, -24, 1511, { .fields = { 120, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 144, -24, 1519, { .fields = { 120, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 152, -24, 1535, { .fields = { 120, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 72, -16, 1470, { .fields = { 120, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 80, -16, 1478, { .fields = { 120, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 88, -16, 1486, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 96, -16, 1493, { .fields = { 120, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 104, -16, 1501, { .fields = { 120, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 112, -16, 1509, { .fields = { 120, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 120, -16, 1516, { .fields = { 120, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 128, -16, 1524, { .fields = { 120, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 136, -16, 1533, { .fields = { 112, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 144, -16, 1541, { .fields = { 120, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 152, -16, 1550, { .fields = { 120, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 88, -8, 1499, { .fields = { 120, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 96, -8, 1507, { .fields = { 120, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 104, -8, 1515, { .fields = { 120, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 112, -8, 1522, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 120, -8, 1527, { .fields = { 112, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 128, -8, 1532, { .fields = { 112, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 136, -8, 1537, { .fields = { 112, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 144, -8, 1543, { .fields = { 112, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 152, -8, 1552, { .fields = { 112, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 88, 0, 1506, { .fields = { 112, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 96, 0, 1513, { .fields = { 112, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 104, 0, 1518, { .fields = { 112, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 112, 0, 1524, { .fields = { 112, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 120, 0, 1533, { .fields = { 112, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 128, 0, 1541, { .fields = { 112, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 136, 0, 1549, { .fields = { 112, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 144, 0, 1558, { .fields = { 112, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 152, 0, 1566, { .fields = { 112, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 88, 8, 1526, { .fields = { 112, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 96, 8, 1533, { .fields = { 112, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 104, 8, 1532, { .fields = { 112, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 112, 8, 1547, { .fields = { 112, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 120, 8, 1555, { .fields = { 112, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 128, 8, 1555, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 136, 8, 1571, { .fields = { 112, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 144, 8, 1578, { .fields = { 104, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 152, 8, 1579, { .fields = { 104, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, -64, -88, 2408, { .fields = { 32, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -24, -88, 2400, { .fields = { 112, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -24, -40, 2400, { .fields = { 120, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 72, 112 } }, 0, -120, 1400, { .fields = { 56, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 112 } }, -8, -120, 1510, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 112 } }, -16, -120, 2087, { .fields = { 48, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 72, 112 } }, -160, -120, 1250, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 104 } }, -88, -112, 1500, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 96 } }, -80, -104, 1750, { .fields = { 40, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 88 } }, -72, -96, 2077, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 104, 72 } }, -104, -32, 1000, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, 0, -32, 1000, { .fields = { 24, 168 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_night_motel_balcony_80185404[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 183, 0, 0, { 1, 0 } },
    { 183, 2, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteDrawArea D_dryfield_night_motel_balcony_80185424[2] = {
    { { 0, 0, 317, 87 }, 3000 },
    { { 0, 0, 0, 0 }, SPRITE_DRAW_AREA_END },
};

SpriteBatch D_dryfield_night_motel_balcony_80185438[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_night_motel_balcony_80185448[12] = {
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -32, 0, 1479, { .fields = { 120, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -24, 0, 1295, { .fields = { 112, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -8, 0, 1481, { .fields = { 112, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 8, -8, 1508, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 32, -8, 1537, { .fields = { 104, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 48, -8, 1567, { .fields = { 112, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 64, -8, 1600, { .fields = { 112, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 80, -8, 1633, { .fields = { 112, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 96, -8, 1669, { .fields = { 112, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 112, -16, 1682, { .fields = { 112, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 128, -16, 1719, { .fields = { 112, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 144, -8, 1755, { .fields = { 104, 32 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_night_motel_balcony_80185538[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 12, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_night_motel_balcony_80185550[54] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 96, 112, 428, { .fields = { 104, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -64, 24, 2957, { .fields = { 40, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -40, 16, 2861, { .fields = { 56, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -32, 16, 2286, { .fields = { 56, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -64, 32, 2560, { .fields = { 24, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -24, 24, 1808, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -64, 40, 2079, { .fields = { 16, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -16, 32, 1517, { .fields = { 72, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -8, 32, 1365, { .fields = { 64, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -64, 48, 1652, { .fields = { 40, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -40, 48, 1652, { .fields = { 48, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -64, 56, 1426, { .fields = { 24, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -32, 56, 1426, { .fields = { 24, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 0, 40, 1194, { .fields = { 64, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 8, 48, 1067, { .fields = { 64, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 16, 48, 980, { .fields = { 64, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -64, 64, 1272, { .fields = { 24, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -24, 64, 1272, { .fields = { 32, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -64, 72, 1202, { .fields = { 16, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -24, 72, 1202, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -64, 80, 1055, { .fields = { 16, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -16, 80, 1055, { .fields = { 24, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -64, 88, 975, { .fields = { 16, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -16, 88, 975, { .fields = { 16, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 24, 56, 901, { .fields = { 72, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -64, 96, 908, { .fields = { 16, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -16, 96, 908, { .fields = { 24, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 16, 96, 900, { .fields = { 32, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 32, 64, 833, { .fields = { 72, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 40, 72, 770, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 48, 72, 734, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -64, 104, 851, { .fields = { 8, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -16, 104, 851, { .fields = { 16, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 24, 104, 852, { .fields = { 24, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -64, 112, 802, { .fields = { 8, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -16, 112, 802, { .fields = { 8, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 32, 112, 808, { .fields = { 32, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 56, 80, 694, { .fields = { 72, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 64, 88, 658, { .fields = { 72, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 72, 96, 625, { .fields = { 56, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 80, 96, 611, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 88, 104, 444, { .fields = { 80, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 88 } }, -160, -120, 700, { .fields = { 96, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, -128, -120, 750, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 80 } }, -160, -32, 700, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, -128, -32, 750, { .fields = { 112, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 72 } }, -160, 48, 700, { .fields = { 64, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -128, 48, 750, { .fields = { 96, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -160, -120, 495, { .fields = { 80, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -144, -112, 501, { .fields = { 80, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -128, -96, 526, { .fields = { 56, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -40, 16, 2861, { .fields = { 32, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -8, 16, 2860, { .fields = { 32, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, 24, 16, 2860, { .fields = { 32, 64 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_night_motel_balcony_80185988[6] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 42, 0, 0, { 3, 0 } },
    { 42, 6, 0, 0, { 0, 0 } },
    { 48, 3, 0, 0, { 2, 0 } },
    { 51, 3, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_night_motel_balcony_801859B8[38] = {
    { 143, 0x3FC0, { .fields = { 56, 40 } }, -160, 80, 351, { .fields = { 0, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 48 } }, -104, 72, 369, { .fields = { 0, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 48 } }, -40, 72, 394, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 72, 56 } }, 24, 64, 427, { .fields = { 120, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 96, 72, 450, { .fields = { 112, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -48, 24, 3491, { .fields = { 72, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -40, 24, 2533, { .fields = { 72, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -32, 24, 2009, { .fields = { 72, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -24, 24, 1667, { .fields = { 88, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -16, 32, 1417, { .fields = { 88, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -8, 32, 1239, { .fields = { 96, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 0, 40, 1107, { .fields = { 96, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 16, 40, 971, { .fields = { 104, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 8, 40, 997, { .fields = { 104, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 24, 48, 902, { .fields = { 80, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 32, 48, 833, { .fields = { 112, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 40, 48, 787, { .fields = { 120, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 48, 56, 742, { .fields = { 80, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 56, 56, 724, { .fields = { 72, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 64, 56, 675, { .fields = { 80, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -56, 24, 3724, { .fields = { 56, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -56, 32, 3740, { .fields = { 32, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -56, 40, 2800, { .fields = { 0, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -56, 48, 1729, { .fields = { 8, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 8 } }, -56, 56, 1513, { .fields = { 120, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 72, 8 } }, -56, 64, 1193, { .fields = { 112, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -56, 72, 1050, { .fields = { 16, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, -16, 72, 1050, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 176 } }, -160, -120, 1176, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 176 } }, -120, -120, 1219, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 168 } }, -112, -112, 1216, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 152 } }, -104, -96, 1514, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 128 } }, -96, -72, 1714, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 96 } }, -88, -48, 1794, { .fields = { 64, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -80, -32, 1632, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -48, 24, 3724, { .fields = { 24, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -16, 24, 3720, { .fields = { 32, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 8, 24, 3720, { .fields = { 40, 240 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_night_motel_balcony_80185CB0[7] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 5, 0, 0, { 0, 0 } },
    { 5, 15, 0, 0, { 3, 0 } },
    { 20, 8, 0, 0, { 2, 0 } },
    { 28, 7, 0, 0, { 4, 0 } },
    { 35, 3, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_night_motel_balcony_80185CE8[74] = {
    { 142, 0x3FC0, { .fields = { 80, 8 } }, -32, -16, 2580, { .fields = { 16, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 80, 8 } }, -32, -8, 2577, { .fields = { 16, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 80, 8 } }, -32, 0, 2244, { .fields = { 16, 88 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 32, 56 } }, -128, 8, 575, { .fields = { 72, 56 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 32, 56 } }, -128, 64, 575, { .fields = { 64, 168 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 32, 56 } }, -96, 8, 575, { .fields = { 64, 112 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 32, 56 } }, -96, 64, 575, { .fields = { 96, 168 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 56 } }, -64, 8, 575, { .fields = { 104, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 56 } }, -64, 64, 575, { .fields = { 0, 152 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 32, 56 } }, -24, 8, 575, { .fields = { 96, 112 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 32, 56 } }, -24, 64, 575, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 32, 56 } }, 8, 8, 575, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 32, 56 } }, 8, 64, 575, { .fields = { 16, 56 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 32, 56 } }, 40, 8, 575, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 24, 56 } }, 72, 8, 575, { .fields = { 8, 112 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 32, 56 } }, 40, 64, 575, { .fields = { 16, 168 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 24, 56 } }, 72, 64, 575, { .fields = { 32, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -80, -64, 1037, { .fields = { 16, 96 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 8, 56 } }, -72, -72, 1133, { .fields = { 64, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -64, -64, 1249, { .fields = { 32, 96 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 8, 56 } }, -56, -64, 1391, { .fields = { 56, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -48, -72, 1569, { .fields = { 120, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -40, -72, 1800, { .fields = { 112, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -32, -72, 2064, { .fields = { 72, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 40, -56, 1838, { .fields = { 24, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 48, -64, 1402, { .fields = { 104, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 56, -24, 1126, { .fields = { 8, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 64, -72, 872, { .fields = { 96, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 72, -64, 880, { .fields = { 72, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 48, 24, 1625, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 72, 56 } }, -80, 64, 1125, { .fields = { 40, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, 8, 16, 1875, { .fields = { 64, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, -48, 24, 1625, { .fields = { 56, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 8 } }, -56, 32, 1524, { .fields = { 48, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 8 } }, 0, 32, 1524, { .fields = { 0, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 8 } }, -64, 40, 1375, { .fields = { 40, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 8 } }, -72, 48, 1325, { .fields = { 40, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 72, 8 } }, -8, 48, 1325, { .fields = { 24, 64 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 72, 8 } }, -80, 56, 1125, { .fields = { 96, 224 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 72, 8 } }, -8, 56, 1125, { .fields = { 80, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 80, 56 } }, -8, 64, 1125, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 88, 56 } }, 72, 64, 1125, { .fields = { 16, 96 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 8 } }, 0, 8, 2000, { .fields = { 64, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, -40, 8, 2000, { .fields = { 0, 208 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 56, 8 } }, -48, 16, 1875, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 64, 8 } }, 0, 40, 1375, { .fields = { 104, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 0, 24, 1625, { .fields = { 80, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 16, 24, 1625, { .fields = { 0, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 32, 24, 1625, { .fields = { 0, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -144, 64, 1125, { .fields = { 80, 184 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 56 } }, -160, 64, 1125, { .fields = { 48, 168 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 56 } }, -128, 64, 1125, { .fields = { 48, 56 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 56 } }, -112, 64, 1125, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 56 } }, -96, 64, 1125, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 80, 8 } }, -32, -16, 2601, { .fields = { 16, 56 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 80, 8 } }, -32, -8, 2602, { .fields = { 104, 248 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 80, 8 } }, -32, 0, 2269, { .fields = { 24, 248 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 8 } }, 32, 0, 2294, { .fields = { 104, 248 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 144 } }, 40, -112, 1843, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 168 } }, 48, -120, 1427, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 184 } }, 56, -120, 1160, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 192 } }, 64, -120, 970, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 96 } }, 72, -120, 862, { .fields = { 64, 160 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 96 } }, 72, -24, 862, { .fields = { 104, 96 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 112 } }, -32, -96, 2110, { .fields = { 56, 144 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 128 } }, -40, -104, 1825, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 144 } }, -48, -112, 1594, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 160 } }, -56, -120, 1416, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 168 } }, -64, -120, 1274, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 184 } }, -72, -120, 1160, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 192 } }, -80, -120, 1064, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 192 } }, -88, -120, 982, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 72, 96 } }, -160, -120, 950, { .fields = { 112, 144 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 72, 96 } }, -160, -24, 950, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_night_motel_balcony_801862B0[8] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 3, 0, 0, { 3, 0 } },
    { 3, 14, 0, 0, { 0, 0 } },
    { 17, 12, 0, 0, { 5, 0 } },
    { 29, 25, 0, 0, { 1, 0 } },
    { 54, 3, 0, 0, { 4, 0 } },
    { 57, 17, 0, 0, { 2, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteDrawArea D_dryfield_night_motel_balcony_801862F0[2] = {
    { { 0, 0, 237, 239 }, 1125 },
    { { 0, 0, 0, 0 }, SPRITE_DRAW_AREA_END },
};

SpriteSource D_dryfield_night_motel_balcony_80186304[74] = {
    { 142, 0x3FC0, { .fields = { 8, 216 } }, -40, -96, 1170, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 232 } }, -48, -112, 1088, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 240 } }, -56, -120, 996, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 240 } }, -64, -120, 945, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 240 } }, -72, -120, 856, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 240 } }, -80, -120, 826, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 240 } }, -88, -120, 779, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 240 } }, -104, -120, 970, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 240 } }, -120, -120, 677, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 240 } }, -136, -120, 626, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 240 } }, -160, -120, 551, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 96 } }, 80, -120, 1043, { .fields = { 96, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 240 } }, 88, -120, 977, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, 80, 64, 755, { .fields = { 104, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 240 } }, 96, -120, 893, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 80, 8 } }, -96, 112, 760, { .fields = { 24, 8 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 72, 8 } }, -16, 112, 760, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 72, 8 } }, 56, 112, 760, { .fields = { 40, 240 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 72, 8 } }, -88, 104, 807, { .fields = { 32, 32 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 64, 8 } }, -16, 104, 807, { .fields = { 40, 24 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 72, 8 } }, 48, 104, 807, { .fields = { 32, 16 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 72, 8 } }, -72, 96, 913, { .fields = { 48, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 8 } }, 0, 96, 913, { .fields = { 8, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, 64, 96, 913, { .fields = { 40, 248 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 56, 8 } }, -56, 88, 1100, { .fields = { 56, 232 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 56, 8 } }, 0, 88, 1100, { .fields = { 56, 224 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 8 } }, 56, 88, 1100, { .fields = { 64, 216 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 24 } }, -40, 48, 1243, { .fields = { 64, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 8, 56, 1243, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 72, 24 } }, 24, 48, 1243, { .fields = { 40, 144 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 64, 8 } }, -40, 72, 1237, { .fields = { 40, 96 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 72, 8 } }, 24, 72, 1237, { .fields = { 32, 88 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 72, 8 } }, -48, 80, 1212, { .fields = { 32, 104 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 72, 8 } }, 24, 80, 1212, { .fields = { 32, 112 } }, 128, 128, 128, 0 },
    { 141, 0x4000, { .fields = { 80, 8 } }, -56, 80, 1085, { .fields = { 24, 80 } }, 128, 128, 128, 0 },
    { 141, 0x4000, { .fields = { 72, 8 } }, 24, 80, 1080, { .fields = { 32, 48 } }, 128, 128, 128, 0 },
    { 141, 0x4000, { .fields = { 64, 8 } }, -40, 72, 1215, { .fields = { 40, 40 } }, 128, 128, 128, 0 },
    { 141, 0x4000, { .fields = { 64, 8 } }, 24, 72, 1215, { .fields = { 40, 56 } }, 128, 128, 128, 0 },
    { 141, 0x4000, { .fields = { 64, 8 } }, -40, 64, 1217, { .fields = { 40, 72 } }, 128, 128, 128, 0 },
    { 141, 0x4000, { .fields = { 72, 8 } }, 24, 64, 1217, { .fields = { 32, 64 } }, 128, 128, 128, 0 },
    { 141, 0x4000, { .fields = { 64, 24 } }, -40, 40, 1225, { .fields = { 48, 168 } }, 128, 128, 128, 0 },
    { 141, 0x4000, { .fields = { 72, 24 } }, 24, 40, 1225, { .fields = { 40, 120 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 8 } }, -160, 32, 507, { .fields = { 88, 248 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 8 } }, -152, 88, 525, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 152 } }, -40, -64, 1145, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 160 } }, -48, -72, 1080, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 160 } }, -56, -80, 986, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 168 } }, -64, -88, 910, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 141, 0x4000, { .fields = { 8, 128 } }, -80, -96, 769, { .fields = { 120, 128 } }, 128, 128, 128, 0 },
    { 141, 0x4000, { .fields = { 8, 128 } }, -88, -104, 747, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 141, 0x4000, { .fields = { 16, 128 } }, -128, -96, 596, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 128 } }, -96, -104, 706, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 16, 128 } }, -112, -104, 654, { .fields = { 0, 128 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 152 } }, -136, -88, 560, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 144 } }, -144, -80, 540, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 141, 0x4000, { .fields = { 8, 120 } }, -152, -72, 521, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 56 } }, -160, -64, 379, { .fields = { 104, 168 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 16, 24 } }, -88, 40, 774, { .fields = { 56, 200 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 160 } }, -72, -80, 846, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 16, 24 } }, -88, 88, 738, { .fields = { 112, 232 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 16, 48 } }, -104, 72, 664, { .fields = { 72, 160 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 16, 48 } }, -120, 72, 622, { .fields = { 56, 152 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 16, 48 } }, -136, 64, 579, { .fields = { 72, 208 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 48 } }, -144, 64, 532, { .fields = { 88, 208 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 16 } }, -160, 104, 514, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 8 } }, -160, 56, 390, { .fields = { 72, 240 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 144 } }, 152, -120, 549, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 144 } }, 144, -104, 573, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 144 } }, 136, -104, 601, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 112 } }, 128, -72, 659, { .fields = { 48, 144 } }, 128, 128, 128, 0 },
    { 141, 0x4000, { .fields = { 8, 120 } }, 120, -80, 696, { .fields = { 112, 128 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 144 } }, 112, -80, 767, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 152 } }, 104, -64, 823, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 48 } }, 96, -32, 887, { .fields = { 88, 160 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_night_motel_balcony_801868CC[6] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 15, 0, 0, { 3, 0 } },
    { 15, 19, 0, 0, { 0, 0 } },
    { 34, 8, 0, 0, { 2, 0 } },
    { 42, 32, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteDrawArea D_dryfield_night_motel_balcony_801868FC[2] = {
    { { 0, 0, 263, 239 }, 875 },
    { { 0, 0, 0, 0 }, SPRITE_DRAW_AREA_END },
};

SpriteSource D_dryfield_night_motel_balcony_80186910[102] = {
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -160, 72, 511, { .fields = { 96, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -144, 80, 433, { .fields = { 32, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -112, 88, 484, { .fields = { 120, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -112, 104, 485, { .fields = { 48, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -64, 64, 622, { .fields = { 64, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 56 } }, -56, 64, 576, { .fields = { 64, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, 8, 72, 516, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 40 } }, 48, 80, 474, { .fields = { 16, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 32 } }, 96, 88, 449, { .fields = { 0, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 144, 96, 427, { .fields = { 16, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -88, 80, 721, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -80, 80, 716, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -72, 88, 708, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -64, 96, 713, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -56, 104, 627, { .fields = { 8, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -48, 96, 715, { .fields = { 8, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 16, 24, 1189, { .fields = { 40, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 8, 24, 1085, { .fields = { 80, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 0, 32, 1000, { .fields = { 88, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -8, 32, 925, { .fields = { 80, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -16, 40, 862, { .fields = { 32, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -24, 40, 601, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -32, 48, 608, { .fields = { 8, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 8 } }, 24, 48, 1175, { .fields = { 72, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 80, 8 } }, 16, 56, 1067, { .fields = { 48, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 104, 8 } }, 8, 64, 978, { .fields = { 24, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 8 } }, 56, 80, 839, { .fields = { 88, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 8 } }, 8, 72, 903, { .fields = { 72, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, 64, 72, 903, { .fields = { 80, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 48, 8, 1830, { .fields = { 8, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 40, 8, 1672, { .fields = { 16, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 32, 8, 1469, { .fields = { 80, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 24, 16, 1314, { .fields = { 16, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, 48, 24, 1701, { .fields = { 72, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 8 } }, 40, 32, 1478, { .fields = { 56, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 8 } }, 32, 40, 1328, { .fields = { 56, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 88, 16, 2205, { .fields = { 32, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 96, 24, 2125, { .fields = { 56, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -48, 0, 2108, { .fields = { 0, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -32, 0, 2040, { .fields = { 16, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -16, 0, 1977, { .fields = { 48, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, 0, 8, 1932, { .fields = { 112, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, 24, 8, 1877, { .fields = { 112, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -16, -16, 2703, { .fields = { 96, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 0, -8, 2631, { .fields = { 24, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 16, -24, 2534, { .fields = { 56, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -104, 0, 2402, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -88, 0, 2317, { .fields = { 120, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -72, 0, 2238, { .fields = { 112, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -160, -8, 2701, { .fields = { 96, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -144, 0, 2595, { .fields = { 112, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -128, 0, 2517, { .fields = { 112, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -112, 0, 2447, { .fields = { 112, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -72, -40, 2915, { .fields = { 88, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -120, -40, 3206, { .fields = { 72, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -112, -48, 3147, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -96, -48, 3264, { .fields = { 112, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -80, -48, 3562, { .fields = { 112, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -136, -24, 3345, { .fields = { 48, 224 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 88, 8 } }, 24, 32, 1535, { .fields = { 32, 168 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 104, 8 } }, 8, 40, 1359, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 112, 8 } }, 0, 48, 1225, { .fields = { 8, 8 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 120, 8 } }, -8, 56, 1117, { .fields = { 0, 24 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 96, 8 } }, 32, 80, 889, { .fields = { 24, 16 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 56, 8 } }, 72, 88, 615, { .fields = { 56, 80 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 64, 8 } }, -16, 72, 953, { .fields = { 48, 72 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 72, 8 } }, 48, 72, 953, { .fields = { 40, 64 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 72, 8 } }, -16, 64, 1028, { .fields = { 40, 40 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 56, 8 } }, 56, 64, 1028, { .fields = { 56, 32 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 48, 8 } }, 40, 16, 2055, { .fields = { 64, 56 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 64, 8 } }, 32, 24, 1726, { .fields = { 48, 48 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, 48, 8, 1871, { .fields = { 32, 216 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 32 } }, 40, 8, 1692, { .fields = { 88, 224 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 32 } }, 16, 24, 1239, { .fields = { 120, 224 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 40 } }, 8, 24, 1135, { .fields = { 72, 88 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 32 } }, 0, 40, 1038, { .fields = { 112, 224 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 40 } }, -8, 40, 978, { .fields = { 64, 40 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 40 } }, -16, 48, 910, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 40 } }, -24, 48, 746, { .fields = { 72, 48 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 40 } }, -32, 48, 746, { .fields = { 80, 88 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 32 } }, -40, 56, 757, { .fields = { 40, 152 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 32 } }, -48, 56, 767, { .fields = { 48, 120 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, -56, 64, 685, { .fields = { 24, 80 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, -64, 64, 695, { .fields = { 88, 144 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 40 } }, 32, 8, 1500, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 32 } }, 24, 16, 1350, { .fields = { 40, 216 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 8 } }, -80, 8, 2326, { .fields = { 88, 88 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, -160, -8, 2805, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 16 } }, -152, 0, 2725, { .fields = { 0, 96 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 16 } }, -136, 0, 2618, { .fields = { 0, 128 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 16 } }, -120, 0, 2520, { .fields = { 0, 144 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 16 } }, -104, 0, 2427, { .fields = { 24, 240 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 16 } }, -88, 0, 2363, { .fields = { 16, 240 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 16 } }, -72, 0, 2263, { .fields = { 16, 24 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 24 } }, -56, 0, 2193, { .fields = { 16, 104 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 16, 16 } }, -40, 8, 2127, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 16, 16 } }, -24, 8, 2062, { .fields = { 112, 192 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 16, 16 } }, -8, 8, 2001, { .fields = { 112, 144 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 16 } }, 8, 8, 1943, { .fields = { 0, 232 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 16 } }, 24, 8, 1889, { .fields = { 0, 216 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 16, 16 } }, 40, 8, 1845, { .fields = { 120, 16 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 32 } }, -136, -24, 3352, { .fields = { 48, 168 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_night_motel_balcony_80187108[19] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 4, 0, 0, { 6, 0 } },
    { 4, 6, 0, 0, { 9, 0 } },
    { 10, 6, 0, 0, { 2, 0 } },
    { 16, 13, 0, 0, { 10, 0 } },
    { 29, 7, 0, 0, { 0, 0 } },
    { 36, 2, 0, 0, { 11, 0 } },
    { 38, 5, 0, 0, { 1, 0 } },
    { 43, 3, 0, 0, { 12, 0 } },
    { 46, 3, 0, 0, { 8, 0 } },
    { 49, 4, 0, 0, { 13, 0 } },
    { 53, 1, 0, 0, { 4, 0 } },
    { 54, 4, 0, 0, { 14, 0 } },
    { 58, 1, 0, 0, { 3, 0 } },
    { 59, 12, 0, 0, { 15, 0 } },
    { 71, 15, 0, 0, { 7, 0 } },
    { 86, 15, 0, 0, { 16, 0 } },
    { 101, 1, 0, 0, { 5, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_night_motel_balcony_801871A0[62] = {
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -104, 0, 4829, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -32, 24, 4295, { .fields = { 48, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -64, 24, 4323, { .fields = { 32, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 16 } }, -104, 24, 3777, { .fields = { 8, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 24 } }, -104, 24, 2457, { .fields = { 48, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -64, 32, 2451, { .fields = { 16, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 80, 8 } }, -112, 40, 2433, { .fields = { 96, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 72, 8 } }, -112, 48, 1821, { .fields = { 104, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -112, 56, 1512, { .fields = { 8, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 8 } }, -88, 56, 1493, { .fields = { 112, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 104, 8 } }, -120, 64, 1237, { .fields = { 72, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 120, 8 } }, -128, 72, 1062, { .fields = { 56, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 72, 8 } }, -128, 80, 1012, { .fields = { 104, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -40, 40, 1522, { .fields = { 56, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -32, 40, 1342, { .fields = { 56, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -24, 40, 1197, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -16, 40, 1081, { .fields = { 104, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -8, 48, 984, { .fields = { 112, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 0, 48, 905, { .fields = { 80, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 8, 48, 838, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 16, 56, 779, { .fields = { 120, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 24, 56, 743, { .fields = { 112, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 32, 56, 721, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 40, 56, 706, { .fields = { 48, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 48, 64, 647, { .fields = { 32, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, -120, 0, 1349, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -96, 56, 1517, { .fields = { 16, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -96, 48, 1620, { .fields = { 8, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, -112, 0, 1534, { .fields = { 120, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -112, 56, 1517, { .fields = { 24, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -112, 64, 1517, { .fields = { 24, 184 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 88, 40 } }, -160, 80, 403, { .fields = { 32, 40 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 88, 48 } }, -72, 72, 469, { .fields = { 40, 184 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 64, 56 } }, 16, 64, 594, { .fields = { 64, 128 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 48, 8 } }, -104, 32, 3778, { .fields = { 120, 192 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 64, 8 } }, -112, 40, 2458, { .fields = { 112, 248 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 80, 8 } }, -112, 48, 1846, { .fields = { 88, 208 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 104, 8 } }, -120, 56, 1518, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 120, 8 } }, -120, 64, 1237, { .fields = { 64, 168 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 80, 8 } }, -128, 80, 1023, { .fields = { 104, 16 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 88, 8 } }, -80, 72, 1063, { .fields = { 96, 176 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 48, 8 } }, -128, 72, 1088, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 24 } }, -96, 16, 4829, { .fields = { 64, 104 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 8 } }, -80, 32, 4027, { .fields = { 80, 32 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 16 } }, -72, 24, 3762, { .fields = { 56, 120 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, -64, 24, 2460, { .fields = { 72, 80 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, -56, 32, 2462, { .fields = { 64, 80 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, -48, 32, 2435, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 32 } }, -40, 32, 1522, { .fields = { 88, 80 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 32 } }, -32, 40, 1493, { .fields = { 96, 80 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 16 } }, -24, 56, 1251, { .fields = { 48, 152 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, -16, 56, 1081, { .fields = { 56, 80 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 16 } }, -8, 64, 985, { .fields = { 104, 112 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 16 } }, 0, 64, 907, { .fields = { 112, 112 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 16 } }, 8, 64, 838, { .fields = { 88, 112 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 16 } }, 16, 64, 825, { .fields = { 80, 112 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 16 } }, 24, 64, 742, { .fields = { 48, 136 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, 32, 56, 721, { .fields = { 104, 232 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 16 } }, 40, 64, 624, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 16 } }, 48, 64, 633, { .fields = { 56, 152 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 16 } }, 56, 64, 639, { .fields = { 96, 112 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 8 } }, 64, 72, 626, { .fields = { 32, 8 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_night_motel_balcony_80187678[14] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 1, 0, 0, { 7, 0 } },
    { 1, 1, 0, 0, { 2, 0 } },
    { 2, 1, 0, 0, { 6, 0 } },
    { 3, 1, 0, 0, { 1, 0 } },
    { 4, 1, 0, 0, { 9, 0 } },
    { 5, 4, 0, 0, { 0, 0 } },
    { 9, 16, 0, 0, { 10, 0 } },
    { 25, 6, 0, 0, { 5, 0 } },
    { 31, 3, 0, 0, { 8, 0 } },
    { 34, 8, 0, 0, { 4, 0 } },
    { 42, 1, 0, 0, { 11, 0 } },
    { 43, 19, 0, 0, { 3, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_night_motel_balcony_801876E8[54] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 32, -56, 2590, { .fields = { 32, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 56, -24, 2409, { .fields = { 48, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 16, -48, 2679, { .fields = { 96, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -16, -40, 2519, { .fields = { 8, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 0, -40, 2519, { .fields = { 16, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 16, -40, 2519, { .fields = { 16, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 32, -40, 2519, { .fields = { 16, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 48, -40, 2371, { .fields = { 8, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -8, -24, 2345, { .fields = { 8, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 8, -24, 2169, { .fields = { 8, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 24, -24, 2249, { .fields = { 0, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 40, -24, 2772, { .fields = { 0, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 0, -8, 2225, { .fields = { 0, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 16, -8, 2879, { .fields = { 32, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, -32, -32, 2423, { .fields = { 120, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, -64, -24, 2334, { .fields = { 104, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -80, -16, 2251, { .fields = { 120, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -64, -8, 2174, { .fields = { 120, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -48, -8, 2174, { .fields = { 120, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -32, -16, 2251, { .fields = { 120, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -16, -16, 2127, { .fields = { 0, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 0, -24, 2313, { .fields = { 8, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 0, -8, 2207, { .fields = { 8, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -72, 0, 2119, { .fields = { 0, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -64, 8, 1938, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -48, 8, 2073, { .fields = { 16, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -32, 0, 2082, { .fields = { 56, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -16, 0, 2979, { .fields = { 56, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 24 } }, -56, 96, 1479, { .fields = { 24, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 72, 24 } }, -128, 96, 1480, { .fields = { 56, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 32 } }, -128, 64, 1619, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 32 } }, -72, 64, 1619, { .fields = { 72, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 24 } }, -128, 40, 1778, { .fields = { 80, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 24 } }, -80, 40, 1778, { .fields = { 80, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, -112, 24, 1912, { .fields = { 32, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -72, 24, 1874, { .fields = { 24, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -72, 8, 1913, { .fields = { 32, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -120, 16, 2077, { .fields = { 56, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 16 } }, -152, -48, 2005, { .fields = { 104, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 16 } }, -144, -32, 2121, { .fields = { 112, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 16 } }, -136, -16, 2251, { .fields = { 0, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 16 } }, -128, 0, 2177, { .fields = { 24, 88 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 64, 24 } }, -128, 96, 1530, { .fields = { 16, 224 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 64, 24 } }, -64, 96, 1532, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 56, 32 } }, -128, 64, 1669, { .fields = { 72, 64 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 56, 32 } }, -72, 64, 1669, { .fields = { 72, 32 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 48, 24 } }, -128, 40, 1828, { .fields = { 32, 200 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 48, 24 } }, -80, 40, 1828, { .fields = { 40, 152 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 40, 16 } }, -112, 24, 1962, { .fields = { 32, 72 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 32, 16 } }, -72, 24, 1924, { .fields = { 24, 40 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 16 } }, -72, 8, 1963, { .fields = { 48, 104 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 40, 24 } }, -128, -40, 2189, { .fields = { 88, 152 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 48, 24 } }, -128, -16, 2333, { .fields = { 80, 224 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 8 } }, -120, 8, 2218, { .fields = { 56, 120 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_night_motel_balcony_80187B20[8] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 14, 0, 0, { 3, 0 } },
    { 14, 14, 0, 0, { 0, 0 } },
    { 28, 9, 0, 0, { 5, 0 } },
    { 37, 5, 0, 0, { 1, 0 } },
    { 42, 9, 0, 0, { 4, 0 } },
    { 51, 3, 0, 0, { 2, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_night_motel_balcony_80187B60[40] = {
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -72, -24, 3382, { .fields = { 88, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 8 } }, -88, -16, 2947, { .fields = { 72, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -80, -8, 2941, { .fields = { 96, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 24 } }, -136, -16, 2646, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -160, 48, 1762, { .fields = { 96, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -152, -8, 2497, { .fields = { 112, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -160, 0, 2580, { .fields = { 104, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -160, 8, 2338, { .fields = { 104, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -160, 16, 2173, { .fields = { 104, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -160, 24, 1928, { .fields = { 96, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -160, 32, 1863, { .fields = { 80, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -160, 40, 1816, { .fields = { 80, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -144, 32, 1747, { .fields = { 80, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -160, 40, 1811, { .fields = { 72, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -160, 48, 1687, { .fields = { 72, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -160, 56, 1599, { .fields = { 72, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -160, 64, 1555, { .fields = { 64, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -160, 72, 1660, { .fields = { 64, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -160, 80, 1629, { .fields = { 88, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -160, 80, 1634, { .fields = { 88, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -160, 88, 2412, { .fields = { 88, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -160, 96, 2312, { .fields = { 88, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -160, 104, 2220, { .fields = { 88, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -160, 112, 2135, { .fields = { 80, 24 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 8 } }, -160, -8, 2775, { .fields = { 104, 56 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 8 } }, -160, 0, 2605, { .fields = { 104, 48 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 8 } }, -160, 8, 2363, { .fields = { 104, 152 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 8 } }, -160, 16, 2198, { .fields = { 104, 144 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 32, 8 } }, -160, 24, 1953, { .fields = { 96, 160 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 32, 8 } }, -160, 32, 1888, { .fields = { 96, 176 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 32, 8 } }, -160, 40, 1836, { .fields = { 96, 168 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 32, 8 } }, -160, 48, 1712, { .fields = { 96, 112 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 32, 8 } }, -160, 56, 1624, { .fields = { 96, 104 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 40, 8 } }, -160, 64, 1580, { .fields = { 88, 120 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 40, 8 } }, -160, 72, 1685, { .fields = { 88, 136 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 40, 8 } }, -160, 80, 1654, { .fields = { 88, 128 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 40, 8 } }, -160, 88, 2437, { .fields = { 88, 40 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 40, 8 } }, -160, 96, 2337, { .fields = { 88, 72 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 40, 8 } }, -160, 104, 2245, { .fields = { 56, 104 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 48, 8 } }, -160, 112, 2160, { .fields = { 80, 240 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_night_motel_balcony_80187E80[8] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 3, 0, 0, { 3, 0 } },
    { 3, 1, 0, 0, { 0, 0 } },
    { 4, 8, 0, 0, { 5, 0 } },
    { 12, 7, 0, 0, { 1, 0 } },
    { 19, 5, 0, 0, { 4, 0 } },
    { 24, 16, 0, 0, { 2, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_night_motel_balcony_80187EC0[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_night_motel_balcony_80187ED0[40] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 64, 88, 3522, { .fields = { 120, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 56, 88, 2356, { .fields = { 104, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 64, 96, 2694, { .fields = { 72, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 32, 96, 0x32D6, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 40, 88, 0x32E5, { .fields = { 104, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 48, 88, 2476, { .fields = { 104, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 56, 88, 2489, { .fields = { 104, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, 64, 96, 2421, { .fields = { 56, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 8 } }, 56, 104, 2102, { .fields = { 48, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 24, 96, 0x3969, { .fields = { 96, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 32, 96, 1198, { .fields = { 104, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 40, 96, 1349, { .fields = { 104, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 48, 96, 2450, { .fields = { 96, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, 56, 104, 2035, { .fields = { 64, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 8 } }, 48, 112, 1311, { .fields = { 40, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -48, 112, 625, { .fields = { 72, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -16, 104, 750, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 8, 96, 950, { .fields = { 112, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 16, 96, 1018, { .fields = { 112, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 24, 96, 1107, { .fields = { 112, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 32, 96, 1129, { .fields = { 112, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 40, 104, 1238, { .fields = { 104, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 8 } }, 48, 112, 1254, { .fields = { 48, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 104, 16, 2229, { .fields = { 112, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 96 } }, 112, 16, 1550, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 120, 72, 1458, { .fields = { 112, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 96, 24, 2356, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 88 } }, 104, 24, 2148, { .fields = { 120, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 112, 48, 1625, { .fields = { 120, 184 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 8 } }, 64, 88, 3647, { .fields = { 112, 248 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 8 } }, 0, 112, 950, { .fields = { 88, 48 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 16 } }, 16, 104, 1053, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, 24, 96, 1132, { .fields = { 104, 24 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, 32, 96, 1154, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, 40, 96, 1374, { .fields = { 104, 72 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, 48, 88, 2501, { .fields = { 104, 48 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 16 } }, 56, 88, 2539, { .fields = { 104, 224 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 40, 8 } }, 64, 96, 2744, { .fields = { 64, 56 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 48, 8 } }, 56, 104, 2137, { .fields = { 56, 32 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 64, 8 } }, 48, 112, 1336, { .fields = { 40, 128 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_night_motel_balcony_801881F0[9] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 3, 0, 0, { 3, 0 } },
    { 3, 6, 0, 0, { 4, 0 } },
    { 9, 6, 0, 0, { 1, 0 } },
    { 15, 8, 0, 0, { 5, 0 } },
    { 23, 3, 0, 0, { 0, 0 } },
    { 26, 3, 0, 0, { 6, 0 } },
    { 29, 11, 0, 0, { 2, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_night_motel_balcony_80188238[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_night_motel_balcony_80188248[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_night_motel_balcony_80188258[44] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 32, -16, 975, { .fields = { 56, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 40, -40, 975, { .fields = { 72, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 160 } }, 48, -40, 975, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 64, -104, 950, { .fields = { 8, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 48, -72, 975, { .fields = { 16, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, -120, 40, 425, { .fields = { 88, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, -112, 40, 425, { .fields = { 80, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, -104, 48, 425, { .fields = { 64, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 64 } }, -96, 56, 525, { .fields = { 96, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -88, 56, 525, { .fields = { 24, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 56 } }, -80, 64, 775, { .fields = { 88, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 56 } }, -72, 64, 775, { .fields = { 88, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 48 } }, -64, 72, 775, { .fields = { 80, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, -56, 80, 975, { .fields = { 64, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, -32, 80, 975, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, -8, 80, 975, { .fields = { 48, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 16, 80, 975, { .fields = { 72, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 32, 80, 975, { .fields = { 64, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -128, 48, 357, { .fields = { 72, 232 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 24, 56 } }, -80, 64, 774, { .fields = { 72, 112 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 16, 64 } }, -96, 56, 524, { .fields = { 120, 160 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 16, 40 } }, -56, 80, 974, { .fields = { 48, 136 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 32, 160 } }, 80, -40, 980, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 32, 72 } }, 48, 48, 980, { .fields = { 32, 160 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 88 } }, 72, -40, 980, { .fields = { 120, 160 } }, 128, 128, 128, 0 },
    { 143, 0x4040, { .fields = { 8, 8 } }, -136, 112, 337, { .fields = { 120, 248 } }, 128, 128, 128, 0 },
    { 143, 0x4040, { .fields = { 8, 8 } }, -128, 112, 337, { .fields = { 64, 248 } }, 128, 128, 128, 0 },
    { 143, 0x4040, { .fields = { 64, 160 } }, 48, -40, 999, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 142, 0x4040, { .fields = { 48, 40 } }, 0, 80, 974, { .fields = { 24, 208 } }, 128, 128, 128, 0 },
    { 142, 0x4040, { .fields = { 24, 56 } }, -80, 64, 774, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 142, 0x4040, { .fields = { 16, 40 } }, -56, 80, 974, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 142, 0x4040, { .fields = { 16, 64 } }, -96, 56, 524, { .fields = { 104, 160 } }, 128, 128, 128, 0 },
    { 143, 0x4040, { .fields = { 24, 80 } }, -120, 40, 424, { .fields = { 96, 160 } }, 128, 128, 128, 0 },
    { 142, 0x4040, { .fields = { 40, 40 } }, -40, 80, 996, { .fields = { 24, 40 } }, 128, 128, 128, 0 },
    { 143, 0x4040, { .fields = { 8, 16 } }, -136, 96, 313, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 143, 0x4040, { .fields = { 8, 16 } }, -128, 96, 337, { .fields = { 80, 240 } }, 128, 128, 128, 0 },
    { 143, 0x4040, { .fields = { 8, 16 } }, -136, 80, 312, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { 143, 0x4040, { .fields = { 8, 16 } }, -128, 80, 336, { .fields = { 48, 232 } }, 128, 128, 128, 0 },
    { 143, 0x4040, { .fields = { 8, 16 } }, -136, 64, 330, { .fields = { 64, 232 } }, 128, 128, 128, 0 },
    { 143, 0x4040, { .fields = { 8, 16 } }, -128, 64, 356, { .fields = { 56, 232 } }, 128, 128, 128, 0 },
    { 143, 0x4040, { .fields = { 8, 16 } }, -136, 32, 378, { .fields = { 32, 232 } }, 128, 128, 128, 0 },
    { 143, 0x4040, { .fields = { 8, 16 } }, -128, 32, 407, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 143, 0x4040, { .fields = { 8, 16 } }, -136, 48, 329, { .fields = { 40, 232 } }, 128, 128, 128, 0 },
    { 143, 0x4040, { .fields = { 8, 16 } }, -128, 48, 355, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_night_motel_balcony_801885C8[6] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 0, 0, 0, { 3, 0 } },
    { 0, 19, 0, 0, { 0, 0 } },
    { 19, 6, 0, 0, { 2, 0 } },
    { 25, 19, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteDrawArea D_dryfield_night_motel_balcony_801885F8[2] = {
    { { 0, 0, 257, 239 }, 999 },
    { { 0, 0, 0, 0 }, SPRITE_DRAW_AREA_END },
};

SpriteSource D_dryfield_night_motel_balcony_8018860C[102] = {
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 40, -32, 1425, { .fields = { 72, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 160 } }, 48, -56, 1425, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 56 } }, 80, -64, 1334, { .fields = { 64, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, 80, 56, 1418, { .fields = { 32, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 64 } }, 96, -8, 1425, { .fields = { 72, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -16, 88, 1425, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, -88, 24, 1425, { .fields = { 16, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -88, 48, 1425, { .fields = { 24, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, -88, 64, 1425, { .fields = { 16, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -88, 88, 1425, { .fields = { 96, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -56, 16, 1425, { .fields = { 88, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, -64, 40, 1425, { .fields = { 40, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -56, 56, 1425, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -80, 80, 1425, { .fields = { 24, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, -32, 48, 1425, { .fields = { 0, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, 16, 24, 1425, { .fields = { 96, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, 32, 32, 1425, { .fields = { 16, 8 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, 32, 48, 1425, { .fields = { 8, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, 32, 64, 1425, { .fields = { 16, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, 32, 80, 1897, { .fields = { 0, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, 72, 48, 1425, { .fields = { 8, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 16 } }, 72, 32, 1425, { .fields = { 48, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -40, 80, 1425, { .fields = { 32, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, -16, 80, 1425, { .fields = { 32, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, -40, 64, 1425, { .fields = { 32, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -8, 64, 1557, { .fields = { 72, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -40, 24, 1425, { .fields = { 40, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, -16, 24, 1425, { .fields = { 8, 120 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 32, 16 } }, -160, 104, 1088, { .fields = { 56, 56 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 32, 16 } }, -128, 104, 1088, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 32, 24 } }, -96, 96, 1149, { .fields = { 64, 176 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 32, 24 } }, -64, 96, 1149, { .fields = { 64, 120 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 24, 24 } }, 24, 96, 1128, { .fields = { 72, 96 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 24, 24 } }, 48, 96, 1128, { .fields = { 80, 72 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 32, 24 } }, -32, 96, 1149, { .fields = { 72, 152 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 24, 24 } }, 0, 96, 1128, { .fields = { 80, 200 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 40, 16 } }, 72, 104, 1043, { .fields = { 24, 176 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 32, 8 } }, 112, 112, 797, { .fields = { 48, 216 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 40, 16 } }, -120, 80, 2775, { .fields = { 32, 200 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 32 } }, -16, 64, 2775, { .fields = { 32, 64 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 32, 24 } }, 0, 72, 2775, { .fields = { 104, 152 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 24, 16 } }, 32, 80, 2775, { .fields = { 40, 120 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 24, 24 } }, -80, 72, 2775, { .fields = { 112, 104 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 24, 24 } }, -56, 72, 2775, { .fields = { 112, 176 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 16, 24 } }, -32, 72, 2775, { .fields = { 120, 200 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 8 } }, -72, 104, 1171, { .fields = { 16, 56 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 32, 16 } }, -136, 96, 1195, { .fields = { 32, 72 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 40, 32 } }, 16, 72, 1424, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, 56, 72, 1424, { .fields = { 72, 216 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 32, 40 } }, -48, 80, 1513, { .fields = { 32, 144 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 32, 40 } }, -16, 80, 1513, { .fields = { 32, 104 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 32 } }, -104, 80, 1171, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 16, 24 } }, -88, 80, 1171, { .fields = { 120, 80 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 24 } }, -72, 80, 1171, { .fields = { 8, 224 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 32 } }, -160, 80, 1048, { .fields = { 24, 32 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 32 } }, -136, 80, 2775, { .fields = { 8, 64 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 32 } }, -112, 80, 2775, { .fields = { 8, 160 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 32, 24 } }, -88, 80, 2775, { .fields = { 32, 184 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 32, 24 } }, -56, 80, 2775, { .fields = { 120, 32 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 32, 24 } }, -24, 80, 2775, { .fields = { 104, 56 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 32, 24 } }, 8, 80, 2775, { .fields = { 112, 128 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 24, 24 } }, 40, 80, 2775, { .fields = { 112, 224 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 8 } }, 48, 88, 1424, { .fields = { 96, 248 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 24, 16 } }, -128, 88, 1210, { .fields = { 48, 104 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 32 } }, -96, 80, 1163, { .fields = { 8, 192 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 32 } }, -72, 72, 1405, { .fields = { 8, 96 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 16, 16 } }, -48, 88, 1341, { .fields = { 64, 72 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 16, 24 } }, 32, 80, 1424, { .fields = { 96, 176 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 16 } }, 40, -32, 1694, { .fields = { 72, 200 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 32, 152 } }, 48, -56, 1424, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 48, 16 } }, 56, 96, 1000, { .fields = { 24, 160 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 56, 24 } }, 104, 96, 1000, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 48 } }, 80, -64, 1000, { .fields = { 48, 56 } }, 128, 128, 128, 0 },
    { 143, 0x4040, { .fields = { 8, 8 } }, -16, 32, 1437, { .fields = { 8, 56 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4040, { .fields = { 8, 8 } }, 0, 32, 1437, { .fields = { 8, 152 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4040, { .fields = { 32, 8 } }, -88, 24, 1437, { .fields = { 24, 96 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4040, { .fields = { 24, 8 } }, -88, 48, 1437, { .fields = { 104, 248 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4040, { .fields = { 16, 32 } }, -88, 80, 1437, { .fields = { 16, 128 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4040, { .fields = { 16, 24 } }, -56, 16, 1437, { .fields = { 88, 24 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4040, { .fields = { 32, 16 } }, -64, 40, 1437, { .fields = { 64, 224 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4040, { .fields = { 32, 8 } }, -88, 64, 1437, { .fields = { 0, 248 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4040, { .fields = { 16, 40 } }, -56, 56, 1437, { .fields = { 80, 216 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4040, { .fields = { 32, 16 } }, -72, 96, 1437, { .fields = { 56, 40 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4040, { .fields = { 16, 8 } }, -72, 80, 1437, { .fields = { 32, 96 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4040, { .fields = { 48, 8 } }, -32, 48, 1437, { .fields = { 16, 192 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4040, { .fields = { 56, 8 } }, -40, 24, 1437, { .fields = { 0, 56 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4040, { .fields = { 16, 80 } }, 16, 24, 1437, { .fields = { 112, 160 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4040, { .fields = { 32, 8 } }, 32, 32, 1437, { .fields = { 24, 88 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4040, { .fields = { 32, 8 } }, 32, 48, 1437, { .fields = { 24, 64 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4040, { .fields = { 32, 8 } }, 32, 64, 1437, { .fields = { 24, 40 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4040, { .fields = { 32, 16 } }, 32, 72, 1437, { .fields = { 56, 24 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4040, { .fields = { 16, 24 } }, 72, 48, 1437, { .fields = { 96, 128 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4040, { .fields = { 16, 24 } }, 88, 48, 1437, { .fields = { 104, 24 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4040, { .fields = { 16, 24 } }, 104, 48, 1437, { .fields = { 104, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4040, { .fields = { 16, 24 } }, 72, 24, 1437, { .fields = { 104, 80 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4040, { .fields = { 16, 24 } }, 88, 24, 1437, { .fields = { 96, 104 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4040, { .fields = { 16, 24 } }, 104, 24, 1437, { .fields = { 104, 200 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4040, { .fields = { 16, 8 } }, -40, 80, 1437, { .fields = { 104, 48 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4040, { .fields = { 16, 16 } }, -24, 80, 1437, { .fields = { 56, 88 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4040, { .fields = { 24, 8 } }, -8, 80, 1437, { .fields = { 16, 136 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4040, { .fields = { 32, 8 } }, -40, 64, 1437, { .fields = { 16, 16 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4040, { .fields = { 24, 8 } }, -8, 64, 1437, { .fields = { 24, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
};

SpriteBatch D_dryfield_night_motel_balcony_80188E04[13] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 0, 0, 0, { 1, 0 } },
    { 0, 5, 0, 0, { 6, 0 } },
    { 5, 0, 0, 0, { 2, 0 } },
    { 5, 23, 0, 0, { 7, 0 } },
    { 28, 10, 0, 0, { 5, 0 } },
    { 38, 7, 0, 0, { 8, 0 } },
    { 45, 9, 0, 0, { 4, 0 } },
    { 54, 8, 0, 0, { 9, 0 } },
    { 62, 6, 0, 0, { 0, 0 } },
    { 68, 5, 0, 0, { 10, 0 } },
    { 73, 29, 0, 0, { 3, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteDrawArea D_dryfield_night_motel_balcony_80188E6C[2] = {
    { { 0, 0, 272, 239 }, 1425 },
    { { 0, 0, 0, 0 }, SPRITE_DRAW_AREA_END },
};

SpriteSource D_dryfield_night_motel_balcony_80188E80[54] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -80, 112, 750, { .fields = { 96, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -96, 104, 750, { .fields = { 16, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -96, 96, 750, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -96, 88, 750, { .fields = { 104, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -112, 88, 750, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -120, 80, 750, { .fields = { 16, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -96, 80, 750, { .fields = { 120, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -120, 72, 730, { .fields = { 120, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -96, 72, 730, { .fields = { 16, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -72, 64, 750, { .fields = { 24, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -48, 64, 700, { .fields = { 32, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 64 } }, -24, 56, 675, { .fields = { 96, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -72, 80, 750, { .fields = { 32, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -72, 96, 750, { .fields = { 32, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -48, 80, 750, { .fields = { 40, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 56 } }, 8, 64, 625, { .fields = { 56, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, 48, 72, 450, { .fields = { 48, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, 96, 72, 450, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 56 } }, 136, 64, 507, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 56 } }, -72, -96, 1110, { .fields = { 64, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -40, -80, 1114, { .fields = { 104, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 72 } }, -72, -40, 1088, { .fields = { 96, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -80, 32, 1028, { .fields = { 56, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -120, -120, 896, { .fields = { 0, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, -72, -120, 896, { .fields = { 112, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, -120, -112, 929, { .fields = { 112, 8 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, -72, -112, 929, { .fields = { 112, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, -128, -104, 1017, { .fields = { 112, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, -80, -104, 1017, { .fields = { 112, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -136, -96, 1081, { .fields = { 8, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, -88, -96, 1081, { .fields = { 112, 16 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, -136, -88, 1154, { .fields = { 112, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, -88, -88, 1154, { .fields = { 112, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, -136, -80, 1343, { .fields = { 120, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, -96, -80, 1343, { .fields = { 112, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, -136, -72, 1366, { .fields = { 120, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, -96, -72, 1366, { .fields = { 104, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, -136, -64, 1558, { .fields = { 112, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, -96, -64, 1558, { .fields = { 112, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, -136, -56, 1694, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, -104, -56, 1694, { .fields = { 104, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, -136, -48, 1644, { .fields = { 112, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, -104, -48, 1644, { .fields = { 104, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, -136, -32, 2242, { .fields = { 112, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, -104, -32, 2242, { .fields = { 112, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, -136, -40, 2019, { .fields = { 112, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -104, -40, 2019, { .fields = { 0, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 32 } }, -80, 40, 958, { .fields = { 16, 88 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 32, 88 } }, -72, -40, 1100, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 32, 16 } }, -160, 32, 293, { .fields = { 24, 232 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 40, 16 } }, -160, 48, 160, { .fields = { 8, 192 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 32, 16 } }, -160, 64, 109, { .fields = { 16, 176 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 16 } }, -160, 80, 83, { .fields = { 16, 48 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 24 } }, -160, 96, 64, { .fields = { 112, 224 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_night_motel_balcony_801892B8[8] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 19, 0, 0, { 3, 0 } },
    { 19, 4, 0, 0, { 0, 0 } },
    { 23, 24, 0, 0, { 5, 0 } },
    { 47, 1, 0, 0, { 1, 0 } },
    { 48, 1, 0, 0, { 4, 0 } },
    { 49, 5, 0, 0, { 2, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_night_motel_balcony_801892F8[8] = {
    { 142, 0x3FC0, { .fields = { 64, 40 } }, -80, -64, 1000, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 112, 48 } }, -80, -24, 1000, { .fields = { 32, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 120, 88 } }, -88, 24, 1000, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 80 } }, 32, 24, 1000, { .fields = { 80, 88 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 64, 40 } }, -80, -64, 1025, { .fields = { 80, 200 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 120, 64 } }, -80, -24, 1025, { .fields = { 32, 88 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 112, 72 } }, -88, 40, 1025, { .fields = { 16, 168 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 56, 64 } }, 24, 40, 1025, { .fields = { 24, 88 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_night_motel_balcony_80189398[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 4, 0, 0, { 1, 0 } },
    { 4, 4, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_night_motel_balcony_801893B8[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_night_motel_balcony_801893C8[16] = {
    { 142, 0x3FC0, { .fields = { 104, 240 } }, -160, -120, 709, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 56 } }, -40, -88, 1192, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 32 } }, -40, -32, 1150, { .fields = { 8, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, -40, 0, 1150, { .fields = { 8, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 32 } }, -40, 40, 1150, { .fields = { 8, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, -40, 72, 1150, { .fields = { 0, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, -40, 96, 1150, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 224 } }, -56, -104, 1038, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 104, 240 } }, -160, -120, 708, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 16, 224 } }, -56, -104, 1038, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 32 } }, -40, -88, 1149, { .fields = { 16, 224 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 32 } }, -40, -56, 1149, { .fields = { 40, 224 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 40 } }, -40, -24, 1149, { .fields = { 8, 96 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 40 } }, -40, 16, 1149, { .fields = { 8, 56 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 32 } }, -40, 56, 1149, { .fields = { 24, 224 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 32 } }, -40, 88, 1149, { .fields = { 32, 224 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_night_motel_balcony_80189508[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 8, 0, 0, { 1, 0 } },
    { 8, 8, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_night_motel_balcony_80189528[32] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 48, 72, 3500, { .fields = { 72, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 88, 72, 3500, { .fields = { 80, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, 56, 48, 3500, { .fields = { 24, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 72, 64 } }, -96, -80, 875, { .fields = { 72, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 72, 32 } }, -104, 40, 875, { .fields = { 56, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 88, 48 } }, -120, 72, 875, { .fields = { 16, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -32, 112, 875, { .fields = { 32, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, 8, 112, 875, { .fields = { 16, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 80, 56 } }, -104, -16, 875, { .fields = { 64, 200 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 48, 48 } }, 40, 32, 3475, { .fields = { 80, 136 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 24, 8 } }, 64, 80, 3475, { .fields = { 48, 120 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 80, 72 } }, -128, -120, 862, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 112, 96 } }, -136, -48, 862, { .fields = { 16, 128 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 112, 64 } }, -144, 48, 862, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 8 } }, 48, 72, 3457, { .fields = { 24, 72 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 40, 8 } }, 32, 80, 2334, { .fields = { 56, 192 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 40, 8 } }, 72, 80, 2150, { .fields = { 96, 64 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 24, 8 } }, 88, 72, 2674, { .fields = { 48, 112 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 24, 8 } }, 96, 64, 2927, { .fields = { 72, 64 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 48, 8 } }, 24, 80, 1944, { .fields = { 16, 200 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 56, 8 } }, 72, 80, 1853, { .fields = { 8, 208 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 64, 8 } }, 40, 88, 1698, { .fields = { 8, 128 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 32, 16 } }, 104, 64, 1927, { .fields = { 96, 184 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 40, 24 } }, 120, 56, 3312, { .fields = { 32, 88 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 64, 8 } }, 96, 80, 2849, { .fields = { 0, 216 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 64, 32 } }, -40, 88, 791, { .fields = { 16, 136 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 72, 24 } }, 24, 88, 952, { .fields = { 8, 168 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 64, 24 } }, 96, 88, 925, { .fields = { 8, 64 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 32, 8 } }, 128, 112, 645, { .fields = { 32, 232 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 128 } }, -24, -56, 1125, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 64 } }, 0, -8, 1640, { .fields = { 0, 136 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 80 } }, 16, -8, 1758, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_night_motel_balcony_801897A8[10] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 3, 0, 0, { 6, 0 } },
    { 3, 6, 0, 0, { 1, 0 } },
    { 9, 2, 0, 0, { 4, 0 } },
    { 11, 3, 0, 0, { 0, 0 } },
    { 14, 5, 0, 0, { 5, 0 } },
    { 19, 4, 0, 0, { 3, 0 } },
    { 23, 6, 0, 0, { 7, 0 } },
    { 29, 3, 0, 0, { 2, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_night_motel_balcony_801897F8[2] = {
    { 142, 0x3FC0, { .fields = { 152, 64 } }, 8, 56, 2250, { .fields = { 104, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 168, 184 } }, -160, -64, 2250, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_night_motel_balcony_80189820[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_night_motel_balcony_80189830[72] = {
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 72, -8, 1595, { .fields = { 24, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 80, -8, 1650, { .fields = { 24, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 112, -8, 1702, { .fields = { 24, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 136, -16, 1729, { .fields = { 56, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 64, 8, 1603, { .fields = { 96, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, -16, 0, 1481, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 16, -8, 1516, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 32, -8, 1552, { .fields = { 64, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -32, 0, 1396, { .fields = { 32, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -32, 16, 1446, { .fields = { 96, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -24, 16, 1305, { .fields = { 96, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -16, 16, 1187, { .fields = { 32, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -16, 32, 1099, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -8, 32, 1027, { .fields = { 8, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -8, 48, 861, { .fields = { 0, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -8, 64, 873, { .fields = { 80, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 0, 88, 777, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 8, 64, 779, { .fields = { 48, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 16, 88, 646, { .fields = { 32, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, 8, 112, 616, { .fields = { 104, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 72 } }, -96, -64, 1875, { .fields = { 64, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -48, -24, 1875, { .fields = { 96, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -104, -64, 1875, { .fields = { 56, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, 72, 0, 2000, { .fields = { 96, 8 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 8 } }, 64, -8, 2000, { .fields = { 96, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 8 } }, -8, -8, 1964, { .fields = { 56, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 8 } }, -8, 0, 1950, { .fields = { 88, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, -8, 8, 1950, { .fields = { 96, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 88 } }, -160, 16, 816, { .fields = { 112, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 104 } }, -152, 0, 872, { .fields = { 120, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, -144, 24, 925, { .fields = { 112, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, -136, 40, 960, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -136, -24, 1080, { .fields = { 48, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 120 } }, -128, -24, 1075, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 88 } }, -120, -8, 1202, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, -112, 0, 1354, { .fields = { 104, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 88, 8 } }, -104, 24, 1330, { .fields = { 56, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -40, 16, 1499, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 96, 8 } }, -104, 32, 1333, { .fields = { 56, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 96, 8 } }, -104, 40, 1248, { .fields = { 56, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 112, 8 } }, -112, 48, 1179, { .fields = { 40, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 112, 8 } }, -112, 56, 1104, { .fields = { 40, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 120, 8 } }, -112, 64, 1052, { .fields = { 32, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 120, 8 } }, -112, 72, 1000, { .fields = { 40, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 128, 8 } }, -120, 80, 953, { .fields = { 32, 16 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 128, 8 } }, -120, 88, 911, { .fields = { 24, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 144, 8 } }, -128, 96, 873, { .fields = { 8, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 144, 8 } }, -128, 104, 838, { .fields = { 8, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 144, 8 } }, -128, 112, 806, { .fields = { 8, 208 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 64, 24 } }, -32, 0, 1531, { .fields = { 8, 24 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 16 } }, 32, 0, 1596, { .fields = { 16, 48 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 64, 24 } }, 48, -8, 1666, { .fields = { 32, 176 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 16 } }, 112, -8, 1743, { .fields = { 16, 160 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 24 } }, 128, -16, 1754, { .fields = { 72, 32 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, -32, 0, 1479, { .fields = { 56, 224 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 16 } }, -24, 0, 1400, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 8 } }, -24, 16, 1312, { .fields = { 48, 48 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 16 } }, -24, 24, 1325, { .fields = { 32, 48 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 16 } }, -16, 24, 1200, { .fields = { 8, 120 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, -16, 40, 1101, { .fields = { 56, 80 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 16 } }, -8, 40, 1050, { .fields = { 24, 64 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 32 } }, -8, 56, 1025, { .fields = { 120, 224 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 16 } }, 0, 56, 939, { .fields = { 32, 224 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 32, 16 } }, 0, 72, 875, { .fields = { 24, 240 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 16 } }, 0, 88, 867, { .fields = { 40, 48 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 32, 8 } }, 8, 88, 787, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, 8, 96, 783, { .fields = { 56, 152 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 32, 16 } }, 16, 96, 655, { .fields = { 24, 144 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 40, 8 } }, 16, 112, 624, { .fields = { 112, 192 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 48, 48 } }, -88, -40, 1887, { .fields = { 64, 200 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 8 } }, -88, 8, 1887, { .fields = { 0, 216 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 16 } }, -96, -8, 1887, { .fields = { 24, 192 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_night_motel_balcony_80189DD0[12] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 4, 0, 0, { 6, 0 } },
    { 4, 4, 0, 0, { 1, 0 } },
    { 8, 12, 0, 0, { 8, 0 } },
    { 20, 3, 0, 0, { 0, 0 } },
    { 23, 2, 0, 0, { 5, 0 } },
    { 25, 3, 0, 0, { 2, 0 } },
    { 28, 21, 0, 0, { 9, 0 } },
    { 49, 5, 0, 0, { 4, 0 } },
    { 54, 15, 0, 0, { 7, 0 } },
    { 69, 3, 0, 0, { 3, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_night_motel_balcony_80189E30[93] = {
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -160, -120, 392, { .fields = { 24, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, -160, -104, 433, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 16 } }, -160, -88, 644, { .fields = { 40, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -72, -8, 4018, { .fields = { 48, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 16 } }, 8, 16, 3085, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, -24, 16, 2956, { .fields = { 8, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, -80, 24, 2880, { .fields = { 88, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -40, 16, 2927, { .fields = { 88, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -32, 24, 2987, { .fields = { 88, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 8 } }, -88, 32, 2535, { .fields = { 40, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -32, 24, 2625, { .fields = { 80, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, -32, 32, 2087, { .fields = { 80, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -48, 32, 1988, { .fields = { 104, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 8 } }, -88, 40, 2087, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -24, 40, 2075, { .fields = { 8, 8 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 8 } }, -88, 48, 1700, { .fields = { 96, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -24, 40, 2075, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 80, 8 } }, -96, 48, 1703, { .fields = { 80, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, -16, 48, 1725, { .fields = { 120, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 88, 8 } }, -96, 56, 1498, { .fields = { 64, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, -8, 56, 1513, { .fields = { 120, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, 0, 64, 1387, { .fields = { 120, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, 8, 72, 1053, { .fields = { 112, 16 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 8 } }, 8, 80, 980, { .fields = { 104, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 8 } }, 16, 88, 902, { .fields = { 104, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 80, 8 } }, -128, 96, 795, { .fields = { 80, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 72, 8 } }, -48, 96, 787, { .fields = { 88, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 8 } }, 24, 96, 795, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 80, 8 } }, -136, 104, 750, { .fields = { 72, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 80, 8 } }, -56, 104, 750, { .fields = { 72, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 72, 8 } }, 24, 104, 750, { .fields = { 80, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 72, 8 } }, 32, 112, 715, { .fields = { 80, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 72, 8 } }, -120, 88, 900, { .fields = { 80, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 8 } }, -48, 88, 902, { .fields = { 88, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 72, 8 } }, -120, 80, 987, { .fields = { 80, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 8 } }, -48, 80, 962, { .fields = { 96, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 8 } }, -112, 72, 1050, { .fields = { 88, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 8 } }, -48, 72, 1052, { .fields = { 96, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 8 } }, -104, 64, 1350, { .fields = { 96, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, -48, 64, 1368, { .fields = { 104, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 8 } }, -144, 112, 715, { .fields = { 96, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 8 } }, -88, 112, 715, { .fields = { 96, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 8 } }, -32, 112, 715, { .fields = { 72, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 104 } }, -120, -72, 800, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 160 } }, -152, -80, 722, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 160 } }, -136, -80, 747, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 160 } }, -128, -80, 772, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 104 } }, -120, -40, 1538, { .fields = { 24, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 104 } }, -96, -40, 1685, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 240 } }, -160, -120, 725, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 240 } }, -144, -120, 750, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 240 } }, -136, -120, 796, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 240 } }, -128, -120, 800, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 232 } }, -120, -112, 822, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 40 } }, -72, -8, 4022, { .fields = { 64, 160 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 32 } }, -56, 0, 4073, { .fields = { 48, 160 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 8 } }, -32, 32, 2545, { .fields = { 56, 192 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 8 } }, -24, 40, 2081, { .fields = { 80, 232 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 16, 8 } }, -40, 16, 2928, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 16, 8 } }, -32, 24, 2997, { .fields = { 80, 160 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 48, 8 } }, -80, 24, 2882, { .fields = { 48, 184 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 56, 8 } }, -88, 32, 2535, { .fields = { 40, 192 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 64, 8 } }, -88, 40, 2093, { .fields = { 32, 208 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 80, 8 } }, -96, 48, 1705, { .fields = { 16, 216 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 24, 8 } }, -16, 48, 1729, { .fields = { 64, 88 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 32, 8 } }, -8, 56, 1514, { .fields = { 56, 144 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 32, 8 } }, 0, 64, 1392, { .fields = { 56, 168 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 32, 8 } }, 0, 72, 1055, { .fields = { 56, 200 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 40, 8 } }, 8, 80, 980, { .fields = { 48, 224 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 48, 8 } }, 24, 88, 910, { .fields = { 40, 232 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 56, 8 } }, 32, 96, 795, { .fields = { 24, 32 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 64, 8 } }, 32, 104, 754, { .fields = { 16, 48 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 72, 8 } }, 32, 112, 721, { .fields = { 8, 80 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 72, 8 } }, -120, 88, 900, { .fields = { 8, 96 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 72, 8 } }, -48, 88, 903, { .fields = { 8, 120 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 80, 8 } }, -128, 96, 796, { .fields = { 0, 128 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 80, 8 } }, -48, 96, 793, { .fields = { 0, 152 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 72, 8 } }, -120, 80, 988, { .fields = { 8, 160 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 56, 8 } }, -48, 80, 975, { .fields = { 24, 176 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 64, 8 } }, -112, 72, 1052, { .fields = { 16, 240 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 48, 8 } }, -48, 72, 1062, { .fields = { 24, 8 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 56, 8 } }, -104, 64, 1354, { .fields = { 16, 40 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 48, 8 } }, -48, 64, 1369, { .fields = { 24, 104 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 56, 8 } }, -104, 56, 1500, { .fields = { 16, 112 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 40, 8 } }, -48, 56, 1500, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 64, 8 } }, -144, 112, 715, { .fields = { 32, 24 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 64, 8 } }, -80, 112, 718, { .fields = { 40, 72 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 48, 8 } }, -16, 112, 719, { .fields = { 64, 248 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 64, 8 } }, -136, 104, 767, { .fields = { 48, 16 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 56, 8 } }, -72, 104, 761, { .fields = { 64, 56 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 48, 8 } }, -16, 104, 760, { .fields = { 72, 40 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 16, 8 } }, -24, 32, 2525, { .fields = { 104, 32 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 16, 8 } }, -16, 40, 2091, { .fields = { 120, 232 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_night_motel_balcony_8018A574[14] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 3, 0, 0, { 7, 0 } },
    { 3, 1, 0, 0, { 2, 0 } },
    { 4, 1, 0, 0, { 6, 0 } },
    { 5, 1, 0, 0, { 1, 0 } },
    { 6, 4, 0, 0, { 9, 0 } },
    { 10, 6, 0, 0, { 0, 0 } },
    { 16, 27, 0, 0, { 10, 0 } },
    { 43, 4, 0, 0, { 5, 0 } },
    { 47, 2, 0, 0, { 8, 0 } },
    { 49, 5, 0, 0, { 4, 0 } },
    { 54, 2, 0, 0, { 11, 0 } },
    { 56, 37, 0, 0, { 3, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_night_motel_balcony_8018A5E4[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_night_motel_balcony_8018A5F4[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_night_motel_balcony_8018A604[43] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 72, 104, 1267, { .fields = { 120, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 16 } }, -32, 88, 1426, { .fields = { 8, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 16, 80, 1537, { .fields = { 40, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 40, 72, 1422, { .fields = { 72, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 40, -32, 1694, { .fields = { 48, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 144 } }, 48, -40, 1572, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 144 } }, 56, -40, 1495, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 144 } }, 64, -40, 1507, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, 72, 24, 1350, { .fields = { 96, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 80, 56, 1302, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 88, 88, 1281, { .fields = { 48, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 72, -48, 1415, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 80, -48, 1336, { .fields = { 56, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 88, -56, 1279, { .fields = { 72, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 96, -56, 1225, { .fields = { 64, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 104, -56, 1199, { .fields = { 88, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 64, -40, 1463, { .fields = { 72, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 80, -48, 1416, { .fields = { 88, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 88 } }, 88, -88, 1400, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 104 } }, 96, -104, 1500, { .fields = { 104, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 120, -24, 1694, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 56 } }, 104, 0, 1705, { .fields = { 80, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 48 } }, 64, 56, 1522, { .fields = { 32, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 56, 48, 1425, { .fields = { 16, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 56, 64, 1424, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, 56, 80, 1396, { .fields = { 24, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 80, 88, 1276, { .fields = { 40, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 88, 40, 0x61A7, { .fields = { 32, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 88, 56, 1551, { .fields = { 32, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 88, 72, 1516, { .fields = { 32, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 104, 32, 1728, { .fields = { 40, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 104, 48, 1713, { .fields = { 48, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 104, 64, 1520, { .fields = { 48, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 104, 80, 1487, { .fields = { 56, 240 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 32 } }, 152, -112, 1000, { .fields = { 64, 40 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 32 } }, 128, -96, 1125, { .fields = { 64, 176 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 16 } }, 40, -32, 1625, { .fields = { 64, 72 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 32 } }, 136, -104, 1050, { .fields = { 56, 144 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 32 } }, 112, -88, 1175, { .fields = { 48, 32 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 32 } }, 80, -64, 1318, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 40 } }, 96, -80, 1250, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 32 } }, 64, -56, 1300, { .fields = { 72, 224 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 32 } }, 48, -48, 1460, { .fields = { 88, 224 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_night_motel_balcony_8018A960[9] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 16, 0, 0, { 3, 0 } },
    { 16, 7, 0, 0, { 4, 0 } },
    { 23, 0, 0, 0, { 1, 0 } },
    { 23, 0, 0, 0, { 5, 0 } },
    { 23, 11, 0, 0, { 0, 0 } },
    { 34, 0, 0, 0, { 6, 0 } },
    { 34, 9, 0, 0, { 2, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_night_motel_balcony_8018A9A8[52] = {
    { 143, 0x3FC0, { .fields = { 40, 48 } }, 56, -32, 3251, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 104, 0, 2578, { .fields = { 16, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 120, 0, 2509, { .fields = { 24, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 136, 0, 2444, { .fields = { 24, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 152, 0, 2398, { .fields = { 24, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 88, 0, 2515, { .fields = { 24, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 32, -8, 2142, { .fields = { 48, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 40, 0, 2393, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 48, -16, 2704, { .fields = { 40, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 56, -8, 2897, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 40, 16, 2382, { .fields = { 0, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 56, 16, 2382, { .fields = { 0, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 72, 16, 2329, { .fields = { 8, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 56, 8, 3166, { .fields = { 32, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 72, 8, 2882, { .fields = { 0, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 120 } }, -160, -112, 764, { .fields = { 104, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 96 } }, -160, 8, 764, { .fields = { 88, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, -144, 8, 774, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, -128, 8, 757, { .fields = { 56, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -120, 8, 755, { .fields = { 96, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -112, 8, 755, { .fields = { 72, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -104, 8, 757, { .fields = { 32, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -96, 8, 787, { .fields = { 40, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -144, 96, 762, { .fields = { 32, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -160, 104, 771, { .fields = { 8, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 96 } }, -144, -80, 791, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 88 } }, -120, -72, 751, { .fields = { 48, 88 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 40, 48 } }, 56, -32, 3276, { .fields = { 16, 176 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 120 } }, -160, -112, 789, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 96 } }, -160, 8, 789, { .fields = { 72, 96 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 16 } }, -160, 104, 796, { .fields = { 16, 144 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 88 } }, -144, 8, 799, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 80 } }, -128, 8, 782, { .fields = { 64, 176 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 64 } }, -120, 8, 780, { .fields = { 88, 192 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 40 } }, -112, 8, 780, { .fields = { 80, 192 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, -104, 8, 782, { .fields = { 32, 48 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, -96, 8, 787, { .fields = { 40, 128 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 24 } }, -144, 96, 787, { .fields = { 72, 232 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 16 } }, -128, 88, 799, { .fields = { 0, 120 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 16 } }, -128, 104, 796, { .fields = { 0, 224 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 16 } }, -104, 88, 796, { .fields = { 0, 240 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 16 } }, -104, 104, 796, { .fields = { 0, 104 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 16 } }, -80, 88, 799, { .fields = { 8, 160 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 16 } }, -80, 104, 796, { .fields = { 0, 88 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, -56, 96, 788, { .fields = { 40, 104 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, -48, 96, 788, { .fields = { 40, 80 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 16 } }, -40, 104, 784, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 8 } }, -32, 112, 492, { .fields = { 0, 184 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 8 } }, -120, 80, 776, { .fields = { 0, 48 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 8 } }, -104, 80, 776, { .fields = { 0, 56 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 8 } }, -88, 80, 776, { .fields = { 0, 144 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 8 } }, -72, 80, 776, { .fields = { 0, 152 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_night_motel_balcony_8018ADB8[10] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 1, 0, 0, { 6, 0 } },
    { 1, 4, 0, 0, { 1, 0 } },
    { 5, 1, 0, 0, { 4, 0 } },
    { 6, 9, 0, 0, { 0, 0 } },
    { 15, 10, 0, 0, { 5, 0 } },
    { 25, 2, 0, 0, { 3, 0 } },
    { 27, 1, 0, 0, { 7, 0 } },
    { 28, 24, 0, 0, { 2, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_night_motel_balcony_8018AE08[54] = {
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -104, -40, 1537, { .fields = { 120, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 48 } }, -104, -32, 1594, { .fields = { 16, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -96, 16, 1599, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -64, 16, 1632, { .fields = { 120, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -40, 8, 1667, { .fields = { 40, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -24, 48, 1243, { .fields = { 16, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -16, 48, 1047, { .fields = { 16, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 32, 104, 831, { .fields = { 48, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -160, 112, 682, { .fields = { 120, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -152, 112, 684, { .fields = { 40, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -144, 112, 687, { .fields = { 24, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -136, 112, 681, { .fields = { 32, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -80, 112, 695, { .fields = { 0, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -72, 112, 695, { .fields = { 0, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -64, 112, 695, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 8, 112, 596, { .fields = { 0, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 16, 112, 592, { .fields = { 0, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 24, 112, 604, { .fields = { 0, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 32, 112, 1841, { .fields = { 0, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -136, 24, 1398, { .fields = { 80, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -32, 40, 1262, { .fields = { 120, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -16, 40, 1099, { .fields = { 120, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -8, 48, 1126, { .fields = { 120, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 104 } }, -160, -16, 1218, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 96 } }, -152, -8, 1273, { .fields = { 120, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, -144, 8, 1335, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, -160, 88, 1268, { .fields = { 112, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 72 } }, -128, 48, 1372, { .fields = { 72, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 56 } }, -80, 56, 1392, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 72 } }, -56, 48, 1378, { .fields = { 88, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 64 } }, -24, 56, 1323, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 8, 64, 998, { .fields = { 120, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 16, 80, 795, { .fields = { 64, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 24, 96, 817, { .fields = { 8, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -8, 32, 1128, { .fields = { 16, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 16, 32, 1151, { .fields = { 32, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 40, 24, 1167, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 64, 24, 1176, { .fields = { 16, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 88, 40, 1271, { .fields = { 8, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 32 } }, 96, 24, 1249, { .fields = { 24, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 136, 24, 1287, { .fields = { 8, 32 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 56, 24 } }, 16, 32, 1193, { .fields = { 96, 152 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, 120, 32, 1286, { .fields = { 80, 128 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 16 } }, 128, 32, 1309, { .fields = { 88, 64 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, 152, 24, 1337, { .fields = { 8, 176 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 32 } }, 72, 24, 1231, { .fields = { 56, 184 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 32, 32 } }, 88, 24, 1259, { .fields = { 40, 216 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 24, 24 } }, -8, 32, 1167, { .fields = { 120, 200 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 24 } }, -24, 32, 1192, { .fields = { 0, 56 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 8 } }, -104, -40, 1562, { .fields = { 56, 248 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 64, 48 } }, -104, -32, 1619, { .fields = { 16, 104 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 32, 32 } }, -96, 16, 1624, { .fields = { 88, 224 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 24, 24 } }, -64, 16, 1657, { .fields = { 120, 128 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 32 } }, -40, 8, 1692, { .fields = { 72, 224 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_night_motel_balcony_8018B240[9] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 5, 0, 0, { 3, 0 } },
    { 5, 29, 0, 0, { 4, 0 } },
    { 34, 5, 0, 0, { 1, 0 } },
    { 39, 0, 0, 0, { 5, 0 } },
    { 39, 2, 0, 0, { 0, 0 } },
    { 41, 8, 0, 0, { 6, 0 } },
    { 49, 5, 0, 0, { 2, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_night_motel_balcony_8018B288[25] = {
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 144, 24, 1832, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 24 } }, 104, 16, 1865, { .fields = { 56, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 24 } }, 64, 16, 1896, { .fields = { 48, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 56, 16, 1918, { .fields = { 80, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 48, 16, 1875, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 40, 16, 1867, { .fields = { 88, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 56 } }, -8, -24, 3250, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 32, 8, 3000, { .fields = { 80, 72 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 40, 56 } }, -8, -24, 3000, { .fields = { 88, 56 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, 32, 8, 3000, { .fields = { 112, 112 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 40, 24 } }, 120, 16, 1831, { .fields = { 88, 216 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 8 } }, 48, 16, 1850, { .fields = { 112, 160 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 16 } }, 56, 16, 1883, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 16 } }, 64, 16, 1892, { .fields = { 72, 184 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 48, 24 } }, 80, 16, 1864, { .fields = { 56, 112 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 16 } }, 40, 16, 1850, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 16 } }, 48, 16, 1800, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, 56, 24, 1875, { .fields = { 104, 112 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 16 } }, 48, 32, 1875, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 16 } }, 40, 32, 1862, { .fields = { 80, 200 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 48, 8 } }, 8, 48, 2250, { .fields = { 40, 216 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 32, 24 } }, 8, 24, 2500, { .fields = { 80, 136 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, -40, -8, 2500, { .fields = { 112, 136 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 56 } }, -32, -8, 2500, { .fields = { 120, 112 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 32, 48 } }, -24, 8, 2875, { .fields = { 96, 168 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_night_motel_balcony_8018B47C[8] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 6, 0, 0, { 3, 0 } },
    { 6, 2, 0, 0, { 0, 0 } },
    { 8, 2, 0, 0, { 5, 0 } },
    { 10, 1, 0, 0, { 1, 0 } },
    { 11, 4, 0, 0, { 4, 0 } },
    { 15, 10, 0, 0, { 2, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_night_motel_balcony_8018B4BC[19] = {
    { 143, 0x3FC0, { .fields = { 72, 144 } }, -160, -120, 1250, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 136 } }, -88, -112, 1500, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 120 } }, -80, -104, 1750, { .fields = { 40, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 104 } }, -72, -96, 2077, { .fields = { 120, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, -64, -88, 2408, { .fields = { 88, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -24, -88, 2400, { .fields = { 104, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -24, -40, 2400, { .fields = { 112, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 120 } }, -16, -120, 2087, { .fields = { 48, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 136 } }, -8, -120, 1510, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 160, 136 } }, 0, -120, 1400, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 104, 72 } }, -104, -32, 1000, { .fields = { 64, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 88 } }, 0, -32, 1000, { .fields = { 96, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -24, -32, 2469, { .fields = { 96, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 8 } }, -64, -24, 2519, { .fields = { 112, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 8 } }, -72, -16, 2283, { .fields = { 104, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 80, 8 } }, -80, -8, 1978, { .fields = { 88, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 88, 8 } }, -88, 0, 1753, { .fields = { 80, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 96, 8 } }, -88, 8, 1579, { .fields = { 72, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 104, 8 } }, -96, 16, 1399, { .fields = { 64, 240 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_night_motel_balcony_8018B638[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 10, 0, 0, { 1, 0 } },
    { 10, 2, 0, 0, { 2, 0 } },
    { 12, 7, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteDrawArea D_dryfield_night_motel_balcony_8018B660[2] = {
    { { 0, 0, 319, 107 }, 2412 },
    { { 0, 0, 0, 0 }, SPRITE_DRAW_AREA_END },
};

SpriteBatch D_dryfield_night_motel_balcony_8018B674[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_night_motel_balcony_8018B684[77] = {
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -32, 32, 1822, { .fields = { 40, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -24, 32, 1544, { .fields = { 40, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -16, 32, 1317, { .fields = { 48, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -8, 32, 1139, { .fields = { 104, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 0, 48, 987, { .fields = { 80, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 8, 56, 925, { .fields = { 64, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 16, 56, 811, { .fields = { 48, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 24, 64, 800, { .fields = { 40, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 32, 64, 500, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 40, 64, 500, { .fields = { 40, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 48, 56, 500, { .fields = { 56, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 56, 56, 500, { .fields = { 48, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 40 } }, -160, 80, 351, { .fields = { 56, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 72, 48 } }, -104, 72, 370, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 48 } }, -32, 72, 396, { .fields = { 56, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 72, 56 } }, 24, 64, 402, { .fields = { 56, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 96, 72, 425, { .fields = { 112, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -72, 48, 1504, { .fields = { 56, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -72, 56, 1255, { .fields = { 24, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -64, 56, 1255, { .fields = { 32, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -72, 64, 1068, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -24, 72, 875, { .fields = { 16, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 0, 72, 0x9E59, { .fields = { 24, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 16, 72, 875, { .fields = { 24, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -32, 64, 1043, { .fields = { 16, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 8 } }, -8, 64, 1043, { .fields = { 8, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -40, 56, 1230, { .fields = { 16, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -16, 56, 1230, { .fields = { 16, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -48, 48, 1504, { .fields = { 16, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -24, 48, 1504, { .fields = { 0, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -64, 40, 1944, { .fields = { 0, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -40, 40, 1944, { .fields = { 0, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -64, 48, 1504, { .fields = { 8, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -40, 72, 900, { .fields = { 8, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -56, 72, 900, { .fields = { 16, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -72, 72, 900, { .fields = { 24, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -64, 64, 1068, { .fields = { 24, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -48, 64, 1068, { .fields = { 16, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -56, 56, 1255, { .fields = { 24, 216 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 16 } }, -24, 32, 1517, { .fields = { 40, 104 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, -16, 32, 1269, { .fields = { 96, 232 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 32 } }, -8, 32, 1091, { .fields = { 48, 80 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 32 } }, 0, 40, 955, { .fields = { 88, 200 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 32 } }, 8, 40, 852, { .fields = { 96, 200 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 32 } }, 16, 40, 768, { .fields = { 48, 112 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, 24, 48, 699, { .fields = { 72, 232 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, 32, 48, 683, { .fields = { 80, 232 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, 40, 48, 637, { .fields = { 88, 232 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 16 } }, 48, 56, 592, { .fields = { 40, 120 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 16 } }, 56, 56, 553, { .fields = { 40, 224 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 16 } }, 64, 56, 524, { .fields = { 40, 240 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 8 } }, 72, 64, 483, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 8 } }, -88, 48, 1454, { .fields = { 0, 96 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 8 } }, -64, 48, 1454, { .fields = { 0, 120 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 8 } }, -40, 48, 1454, { .fields = { 0, 104 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 32, 8 } }, -104, 56, 1194, { .fields = { 8, 40 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 32, 8 } }, -72, 56, 1180, { .fields = { 8, 32 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 32, 8 } }, -40, 56, 1180, { .fields = { 8, 48 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 40, 8 } }, -112, 64, 978, { .fields = { 0, 64 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 32, 8 } }, -72, 64, 993, { .fields = { 8, 56 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 8 } }, -40, 64, 993, { .fields = { 16, 24 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 8 } }, -16, 64, 993, { .fields = { 112, 248 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 8 } }, -40, 72, 850, { .fields = { 88, 136 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 8 } }, -16, 72, 850, { .fields = { 64, 136 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 8 } }, -72, 72, 900, { .fields = { 24, 16 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 8 } }, -56, 72, 900, { .fields = { 32, 136 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 8 } }, -112, 72, 900, { .fields = { 16, 128 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 8 } }, -88, 72, 900, { .fields = { 24, 120 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 72 } }, -104, -8, 1460, { .fields = { 112, 72 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 16 } }, -88, 48, 1403, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 48, 16 } }, -80, 24, 3516, { .fields = { 0, 176 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 32, 32 } }, -80, 0, 4433, { .fields = { 24, 144 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 32, 8 } }, -40, 24, 4000, { .fields = { 8, 80 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 32, 8 } }, -8, 24, 4146, { .fields = { 8, 72 } }, 128, 128, 128, 0 },
    { 143, 0x4040, { .fields = { 16, 72 } }, -104, -8, 1485, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4040, { .fields = { 8, 16 } }, -88, 48, 1428, { .fields = { 40, 192 } }, 128, 128, 128, 0 },
    { 143, 0x4040, { .fields = { 32, 32 } }, -80, 0, 4458, { .fields = { 48, 200 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_night_motel_balcony_8018BC88[13] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 12, 0, 0, { 1, 0 } },
    { 12, 5, 0, 0, { 6, 0 } },
    { 17, 22, 0, 0, { 2, 0 } },
    { 39, 29, 0, 0, { 7, 0 } },
    { 68, 2, 0, 0, { 5, 0 } },
    { 70, 1, 0, 0, { 8, 0 } },
    { 71, 1, 0, 0, { 4, 0 } },
    { 72, 1, 0, 0, { 9, 0 } },
    { 73, 1, 0, 0, { 0, 0 } },
    { 74, 2, 0, 0, { 10, 0 } },
    { 76, 1, 0, 0, { 3, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteDrawArea D_dryfield_night_motel_balcony_8018BCF0[2] = {
    { { 73, 0, 233, 239 }, 1700 },
    { { 0, 0, 0, 0 }, SPRITE_DRAW_AREA_END },
};

SpriteSource D_dryfield_night_motel_balcony_8018BD04[36] = {
    { 143, 0x3FC0, { .fields = { 48, 40 } }, -160, 80, 533, { .fields = { 72, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 40 } }, -112, 80, 546, { .fields = { 32, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -64, 72, 550, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 56 } }, -40, 64, 573, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 56 } }, -8, 64, 561, { .fields = { 104, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, 16, 72, 550, { .fields = { 72, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 56 } }, 48, 64, 529, { .fields = { 80, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 48 } }, 96, 72, 451, { .fields = { 72, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, 152, 80, 417, { .fields = { 120, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -160, -40, 2012, { .fields = { 64, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -88, 112, 914, { .fields = { 80, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, -160, -32, 1818, { .fields = { 72, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -160, 8, 1031, { .fields = { 48, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -136, -24, 1524, { .fields = { 40, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -136, 8, 1079, { .fields = { 16, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -104, -8, 1123, { .fields = { 32, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -104, 8, 1079, { .fields = { 32, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -128, 24, 910, { .fields = { 24, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -128, 40, 912, { .fields = { 24, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -120, 56, 918, { .fields = { 24, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -160, 40, 787, { .fields = { 40, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -120, 72, 912, { .fields = { 16, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -120, 88, 907, { .fields = { 24, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -112, 104, 904, { .fields = { 48, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -152, -24, 1637, { .fields = { 56, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -160, -24, 1600, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 32 } }, -144, -32, 1512, { .fields = { 56, 144 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, -128, -32, 1413, { .fields = { 72, 144 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 8 } }, -160, -8, 1390, { .fields = { 48, 96 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 8 } }, -160, 0, 1237, { .fields = { 32, 96 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 16 } }, -160, -40, 1707, { .fields = { 48, 16 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 16 } }, -160, -24, 1567, { .fields = { 40, 64 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 8 } }, -160, -40, 2100, { .fields = { 88, 104 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 16 } }, -144, -40, 2196, { .fields = { 48, 240 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 24 } }, -120, -48, 2351, { .fields = { 48, 176 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 48, 16 } }, -88, -48, 2709, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_night_motel_balcony_8018BFD4[7] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 9, 0, 0, { 0, 0 } },
    { 9, 17, 0, 0, { 3, 0 } },
    { 26, 6, 0, 0, { 2, 0 } },
    { 32, 3, 0, 0, { 4, 0 } },
    { 35, 1, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_night_motel_balcony_8018C00C[39] = {
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -160, 32, 1921, { .fields = { 88, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -160, 56, 1894, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -144, 56, 2059, { .fields = { 88, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -160, 72, 1872, { .fields = { 80, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -144, 72, 2052, { .fields = { 72, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -160, 88, 1875, { .fields = { 72, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -144, 88, 2018, { .fields = { 72, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -160, 104, 1887, { .fields = { 72, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -144, 104, 1980, { .fields = { 72, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -128, 64, 2101, { .fields = { 80, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -112, 64, 2100, { .fields = { 80, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -128, 80, 2046, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -128, 96, 1992, { .fields = { 80, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -112, 80, 2027, { .fields = { 112, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -96, 72, 2052, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 32 } }, -96, 88, 2013, { .fields = { 96, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -120, 96, 2005, { .fields = { 104, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -136, 104, 1992, { .fields = { 72, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, -152, 112, 1980, { .fields = { 72, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -96, -112, 2989, { .fields = { 112, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -80, -120, 3008, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -64, -120, 2980, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -48, -120, 2975, { .fields = { 112, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -32, -120, 3025, { .fields = { 104, 48 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 24 } }, -160, 32, 1946, { .fields = { 88, 96 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 16 } }, -160, 56, 1919, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 16 } }, -160, 72, 1897, { .fields = { 96, 120 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 16 } }, -160, 88, 1900, { .fields = { 96, 224 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 16 } }, -160, 104, 1912, { .fields = { 88, 208 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 16 } }, -144, 56, 2084, { .fields = { 80, 64 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 16 } }, -144, 72, 2077, { .fields = { 80, 80 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 16 } }, -144, 88, 2043, { .fields = { 88, 32 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 16 } }, -128, 64, 2126, { .fields = { 88, 48 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 16 } }, -128, 80, 2071, { .fields = { 88, 192 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 16 } }, -128, 96, 2017, { .fields = { 72, 168 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 8 } }, -144, 104, 2005, { .fields = { 112, 248 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 16 } }, -112, 64, 2125, { .fields = { 80, 152 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, -96, 72, 2077, { .fields = { 104, 24 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 24 } }, -112, 80, 2052, { .fields = { 96, 72 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_night_motel_balcony_8018C318[6] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 15, 0, 0, { 3, 0 } },
    { 15, 4, 0, 0, { 0, 0 } },
    { 19, 5, 0, 0, { 2, 0 } },
    { 24, 15, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteDrawArea D_dryfield_night_motel_balcony_8018C348[2] = {
    { { 0, 0, 2, 1 }, 3500 },
    { { 0, 0, 0, 0 }, SPRITE_DRAW_AREA_END },
};

SpriteSource D_dryfield_night_motel_balcony_8018C35C[88] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 96, -64, 781, { .fields = { 24, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, -160, 56, 386, { .fields = { 80, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -144, 72, 402, { .fields = { 80, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -128, 80, 423, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -112, 72, 453, { .fields = { 56, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -96, 72, 486, { .fields = { 56, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, -80, 72, 523, { .fields = { 64, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -64, 80, 565, { .fields = { 48, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -48, 80, 617, { .fields = { 40, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -32, 72, 696, { .fields = { 120, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -16, 72, 768, { .fields = { 120, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 0, 64, 886, { .fields = { 8, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 16, 64, 1024, { .fields = { 0, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 32, 72, 1184, { .fields = { 112, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 48, 72, 1350, { .fields = { 0, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 56, 72, 1500, { .fields = { 48, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 64, 72, 1764, { .fields = { 56, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 72, 72, 1929, { .fields = { 0, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, 80, 72, 2326, { .fields = { 112, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, 88, 72, 3121, { .fields = { 112, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, 96, 72, 3175, { .fields = { 88, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, -32, 104, 607, { .fields = { 64, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, -32, 112, 438, { .fields = { 64, 16 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 8 } }, 16, 96, 827, { .fields = { 48, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 8 } }, 16, 104, 606, { .fields = { 48, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 8 } }, 16, 112, 477, { .fields = { 48, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 8 } }, 80, 88, 1297, { .fields = { 56, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 8 } }, 80, 96, 827, { .fields = { 48, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 80, 8 } }, 80, 104, 575, { .fields = { 40, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 80, 8 } }, 80, 112, 415, { .fields = { 40, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 88, -112, 602, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 88, -96, 651, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 88, -80, 717, { .fields = { 72, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 64 } }, 104, -120, 370, { .fields = { 72, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 40 } }, 112, -56, 445, { .fields = { 8, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, 120, -16, 486, { .fields = { 8, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 24 } }, 112, 24, 895, { .fields = { 80, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, 128, 48, 748, { .fields = { 32, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, 136, 88, 572, { .fields = { 88, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, 144, 96, 486, { .fields = { 96, 88 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 8 } }, 136, -64, 361, { .fields = { 56, 88 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 64 } }, -160, 56, 356, { .fields = { 112, 192 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 64 } }, -144, 56, 379, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 64 } }, -128, 56, 405, { .fields = { 112, 64 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 64 } }, -112, 56, 434, { .fields = { 96, 192 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 64 } }, -96, 56, 468, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 64 } }, -80, 56, 504, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 48 } }, -64, 64, 552, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 48 } }, -48, 64, 606, { .fields = { 64, 48 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 40 } }, -32, 64, 674, { .fields = { 24, 88 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 40 } }, -16, 64, 750, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 40 } }, 0, 64, 851, { .fields = { 32, 208 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 16, 32 } }, 16, 64, 989, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 16 } }, 32, 80, 985, { .fields = { 64, 240 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 8 } }, 40, 88, 1039, { .fields = { 8, 120 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 16, 24 } }, 112, -120, 517, { .fields = { 112, 152 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 40 } }, 120, -96, 465, { .fields = { 16, 208 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 40 } }, 128, -56, 384, { .fields = { 8, 40 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 48 } }, 136, -16, 379, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 64 } }, 144, 32, 472, { .fields = { 96, 64 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 64, 8 } }, -64, 112, 399, { .fields = { 48, 168 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 80, 8 } }, 0, 112, 399, { .fields = { 32, 160 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 80, 8 } }, 80, 112, 385, { .fields = { 32, 240 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 64, 8 } }, -32, 104, 527, { .fields = { 48, 152 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 72, 8 } }, 32, 104, 541, { .fields = { 40, 128 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 56, 8 } }, 104, 104, 527, { .fields = { 56, 104 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 48, 8 } }, 16, 96, 792, { .fields = { 64, 144 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 48, 8 } }, 64, 96, 797, { .fields = { 64, 136 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 48, 8 } }, 112, 96, 797, { .fields = { 72, 232 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 8 } }, 80, 88, 1146, { .fields = { 32, 40 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 16 } }, 56, 72, 1518, { .fields = { 120, 224 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 24 } }, 64, 72, 1701, { .fields = { 120, 64 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, 72, 72, 1867, { .fields = { 0, 176 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 56, 8 } }, 88, 88, 1146, { .fields = { 64, 112 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 56, 8 } }, 88, 80, 1155, { .fields = { 64, 120 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 16 } }, 80, 72, 2314, { .fields = { 0, 240 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 16 } }, 24, 64, 1139, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 32 } }, 32, 64, 1116, { .fields = { 0, 64 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 32 } }, 40, 64, 1155, { .fields = { 72, 96 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 24 } }, 48, 72, 1312, { .fields = { 48, 232 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 24 } }, 56, 72, 1487, { .fields = { 120, 200 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 16 } }, 64, 80, 1664, { .fields = { 112, 48 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 40, 8 } }, 72, 88, 1284, { .fields = { 80, 224 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 64, 8 } }, 40, 96, 990, { .fields = { 56, 216 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 16 } }, 80, 72, 2373, { .fields = { 112, 16 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 16 } }, 88, 72, 3108, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 16 } }, 96, 72, 2987, { .fields = { 8, 240 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 32, 8 } }, 104, 80, 2250, { .fields = { 16, 248 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_night_motel_balcony_8018CA3C[7] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 40, 0, 0, { 0, 0 } },
    { 40, 29, 0, 0, { 3, 0 } },
    { 69, 7, 0, 0, { 2, 0 } },
    { 76, 8, 0, 0, { 4, 0 } },
    { 84, 4, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_night_motel_balcony_8018CA74[75] = {
    { 143, 0x3FC0, { .fields = { 56, 56 } }, -160, -120, 358, { .fields = { 40, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, -104, -120, 412, { .fields = { 120, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 40 } }, -160, -64, 339, { .fields = { 48, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 32 } }, -160, -24, 325, { .fields = { 24, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -80, 16, 446, { .fields = { 32, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, 32, -24, 1275, { .fields = { 32, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, 104, -16, 1457, { .fields = { 24, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, 32, -16, 1273, { .fields = { 32, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 40, -8, 1251, { .fields = { 56, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 56 } }, 40, 24, 1171, { .fields = { 96, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 32 } }, 40, 80, 1101, { .fields = { 16, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, -40, -120, 837, { .fields = { 80, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, -16, -120, 837, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, 8, -120, 837, { .fields = { 88, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, 32, -120, 837, { .fields = { 88, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, 56, -120, 837, { .fields = { 88, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 16 } }, 80, -120, 837, { .fields = { 80, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 104, -120, 837, { .fields = { 48, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 120, -120, 837, { .fields = { 24, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, -24, -104, 846, { .fields = { 40, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 8 } }, 24, -104, 846, { .fields = { 40, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 8 } }, 88, -104, 846, { .fields = { 48, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, -16, -96, 935, { .fields = { 56, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 8 } }, 32, -96, 935, { .fields = { 24, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 8 } }, 88, -96, 935, { .fields = { 24, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, -16, -88, 980, { .fields = { 40, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 8 } }, 32, -88, 980, { .fields = { 32, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 8 } }, 88, -88, 980, { .fields = { 8, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, -8, -80, 1028, { .fields = { 16, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, 40, -80, 1028, { .fields = { 16, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 8 } }, 88, -80, 1028, { .fields = { 8, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, 0, -72, 1039, { .fields = { 64, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 8 } }, 40, -72, 1039, { .fields = { 32, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, 96, -72, 1039, { .fields = { 40, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, 8, -64, 1147, { .fields = { 56, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, 56, -64, 1147, { .fields = { 40, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, 104, -64, 1147, { .fields = { 48, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, 8, -56, 1082, { .fields = { 40, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, 56, -56, 1082, { .fields = { 64, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, 104, -56, 1082, { .fields = { 112, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, 16, -48, 1161, { .fields = { 56, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, 64, -48, 1161, { .fields = { 64, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, 104, -48, 1161, { .fields = { 48, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, 24, -40, 1241, { .fields = { 64, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, 64, -40, 1241, { .fields = { 64, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, 104, -40, 1241, { .fields = { 48, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, 32, -32, 1454, { .fields = { 48, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, 72, -32, 1454, { .fields = { 48, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 8 } }, 112, -32, 1454, { .fields = { 56, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 8 } }, 88, -24, 1452, { .fields = { 32, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 64 } }, -160, 8, 383, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 64 } }, -112, 8, 383, { .fields = { 96, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 48 } }, -160, 72, 365, { .fields = { 48, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, -112, 72, 365, { .fields = { 64, 168 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 8 } }, 0, -64, 1069, { .fields = { 120, 248 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 32, 64 } }, 40, 16, 1125, { .fields = { 96, 64 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 40, 32 } }, 80, 40, 2223, { .fields = { 8, 120 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 8, 16 } }, 8, -64, 1069, { .fields = { 8, 80 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 8, 24 } }, 16, -64, 1069, { .fields = { 112, 224 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 32 } }, 24, -64, 1069, { .fields = { 24, 200 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 32 } }, 48, -64, 1069, { .fields = { 16, 64 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 32 } }, 72, -64, 1069, { .fields = { 0, 184 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 32, 32 } }, 96, -64, 1069, { .fields = { 120, 216 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 32 } }, 128, -64, 1069, { .fields = { 8, 152 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 32, 24 } }, 88, 72, 3139, { .fields = { 88, 200 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 32, 24 } }, 32, -32, 1125, { .fields = { 104, 104 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 32, 24 } }, 64, -32, 1125, { .fields = { 104, 152 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 24, 24 } }, 40, -8, 1250, { .fields = { 112, 128 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 40, 24 } }, 64, -8, 2102, { .fields = { 88, 176 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 40, 24 } }, 104, -8, 2102, { .fields = { 104, 56 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 32, 24 } }, 80, 16, 3067, { .fields = { 104, 80 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 32, 24 } }, 112, 16, 3067, { .fields = { 112, 32 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 24 } }, 96, -32, 1125, { .fields = { 8, 96 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 24 } }, 120, -32, 1125, { .fields = { 24, 232 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 208, 32 } }, -48, 88, 2000, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_night_motel_balcony_8018D050[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 54, 0, 0, { 1, 0 } },
    { 54, 20, 0, 0, { 2, 0 } },
    { 74, 1, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView D_dryfield_night_motel_balcony_8018D078[39] = {
    { { .empty = D_dryfield_night_motel_balcony_80184580 }, D_dryfield_night_motel_balcony_80184580, NULL },
    { { .elements = D_dryfield_night_motel_balcony_80184590 }, D_dryfield_night_motel_balcony_80185404, D_dryfield_night_motel_balcony_80185424 },
    { { .empty = D_dryfield_night_motel_balcony_80185438 }, D_dryfield_night_motel_balcony_80185438, NULL },
    { { .elements = D_dryfield_night_motel_balcony_80185448 }, D_dryfield_night_motel_balcony_80185538, NULL },
    { { .elements = D_dryfield_night_motel_balcony_80185550 }, D_dryfield_night_motel_balcony_80185988, NULL },
    { { .elements = D_dryfield_night_motel_balcony_801859B8 }, D_dryfield_night_motel_balcony_80185CB0, NULL },
    { { .elements = D_dryfield_night_motel_balcony_80185CE8 }, D_dryfield_night_motel_balcony_801862B0, D_dryfield_night_motel_balcony_801862F0 },
    { { .elements = D_dryfield_night_motel_balcony_80186304 }, D_dryfield_night_motel_balcony_801868CC, D_dryfield_night_motel_balcony_801868FC },
    { { .elements = D_dryfield_night_motel_balcony_80186910 }, D_dryfield_night_motel_balcony_80187108, NULL },
    { { .elements = D_dryfield_night_motel_balcony_801871A0 }, D_dryfield_night_motel_balcony_80187678, NULL },
    { { .elements = D_dryfield_night_motel_balcony_801876E8 }, D_dryfield_night_motel_balcony_80187B20, NULL },
    { { .elements = D_dryfield_night_motel_balcony_80187B60 }, D_dryfield_night_motel_balcony_80187E80, NULL },
    { { .empty = D_dryfield_night_motel_balcony_80187EC0 }, D_dryfield_night_motel_balcony_80187EC0, NULL },
    { { .elements = D_dryfield_night_motel_balcony_80187ED0 }, D_dryfield_night_motel_balcony_801881F0, NULL },
    { { .empty = D_dryfield_night_motel_balcony_80188238 }, D_dryfield_night_motel_balcony_80188238, NULL },
    { { .empty = D_dryfield_night_motel_balcony_80188248 }, D_dryfield_night_motel_balcony_80188248, NULL },
    { { .elements = D_dryfield_night_motel_balcony_80188258 }, D_dryfield_night_motel_balcony_801885C8, D_dryfield_night_motel_balcony_801885F8 },
    { { .elements = D_dryfield_night_motel_balcony_8018860C }, D_dryfield_night_motel_balcony_80188E04, D_dryfield_night_motel_balcony_80188E6C },
    { { .elements = D_dryfield_night_motel_balcony_80188E80 }, D_dryfield_night_motel_balcony_801892B8, NULL },
    { { .elements = D_dryfield_night_motel_balcony_801892F8 }, D_dryfield_night_motel_balcony_80189398, NULL },
    { { .empty = D_dryfield_night_motel_balcony_801893B8 }, D_dryfield_night_motel_balcony_801893B8, NULL },
    { { .elements = D_dryfield_night_motel_balcony_801893C8 }, D_dryfield_night_motel_balcony_80189508, NULL },
    { { .elements = D_dryfield_night_motel_balcony_80189528 }, D_dryfield_night_motel_balcony_801897A8, NULL },
    { { .elements = D_dryfield_night_motel_balcony_801897F8 }, D_dryfield_night_motel_balcony_80189820, NULL },
    { { .elements = D_dryfield_night_motel_balcony_80189830 }, D_dryfield_night_motel_balcony_80189DD0, NULL },
    { { .elements = D_dryfield_night_motel_balcony_80189E30 }, D_dryfield_night_motel_balcony_8018A574, NULL },
    { { .empty = D_dryfield_night_motel_balcony_8018A5E4 }, D_dryfield_night_motel_balcony_8018A5E4, NULL },
    { { .empty = D_dryfield_night_motel_balcony_8018A5F4 }, D_dryfield_night_motel_balcony_8018A5F4, NULL },
    { { .elements = D_dryfield_night_motel_balcony_8018A604 }, D_dryfield_night_motel_balcony_8018A960, NULL },
    { { .elements = D_dryfield_night_motel_balcony_8018A9A8 }, D_dryfield_night_motel_balcony_8018ADB8, NULL },
    { { .elements = D_dryfield_night_motel_balcony_8018AE08 }, D_dryfield_night_motel_balcony_8018B240, NULL },
    { { .elements = D_dryfield_night_motel_balcony_8018B288 }, D_dryfield_night_motel_balcony_8018B47C, NULL },
    { { .elements = D_dryfield_night_motel_balcony_8018B4BC }, D_dryfield_night_motel_balcony_8018B638, D_dryfield_night_motel_balcony_8018B660 },
    { { .empty = D_dryfield_night_motel_balcony_8018B674 }, D_dryfield_night_motel_balcony_8018B674, NULL },
    { { .elements = D_dryfield_night_motel_balcony_8018B684 }, D_dryfield_night_motel_balcony_8018BC88, D_dryfield_night_motel_balcony_8018BCF0 },
    { { .elements = D_dryfield_night_motel_balcony_8018BD04 }, D_dryfield_night_motel_balcony_8018BFD4, NULL },
    { { .elements = D_dryfield_night_motel_balcony_8018C00C }, D_dryfield_night_motel_balcony_8018C318, D_dryfield_night_motel_balcony_8018C348 },
    { { .elements = D_dryfield_night_motel_balcony_8018C35C }, D_dryfield_night_motel_balcony_8018CA3C, NULL },
    { { .elements = D_dryfield_night_motel_balcony_8018CA74 }, D_dryfield_night_motel_balcony_8018D050, NULL },
};

/// Twenty-two point lights shared by nighttime motel balcony room variants 1 and 3.
///
/// Every light contributes in every view. Positions and inner/outer falloff radii
/// use integer world units; RGB intensities have 12 fractional bits (`ONE` is 1.0).
/// Strength is full through the inner radius, then falls with squared distance
/// to zero at the outer radius. The loaded room overlay owns this writable array:
/// coordinate updates parent and compose its transforms, and lighting queries
/// overwrite attenuation. Borrowed pointers must not outlive the overlay.
static WorldCoordPointLight _gDryfieldNightMotelBalconyRooms1And3PointLights[] = {
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -4417, -2000, 2032 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { ONE, ONE, ONE },
        },
        .inner = 1500,
        .outer = 4000,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -6862, -2000, 316 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { ONE, ONE, ONE },
        },
        .inner = 500,
        .outer = 2000,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -2611, -5200, 0x2A75 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { ONE, ONE, ONE },
        },
        .inner = 1000,
        .outer = 2500,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -6888, -5200, 3833 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { ONE, ONE, ONE },
        },
        .inner = 1000,
        .outer = 2500,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -6888, -5200, 5310 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { ONE, ONE, ONE },
        },
        .inner = 1000,
        .outer = 2500,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -6888, -5200, -3212 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { ONE, ONE, ONE },
        },
        .inner = 1000,
        .outer = 2500,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -0x2FD2, -5200, 424 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { ONE, ONE, ONE },
        },
        .inner = 1381,
        .outer = 3827,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -3635, -6200, 7089 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { ONE, ONE, ONE },
        },
        .inner = 2000,
        .outer = 4500,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -3635, -6200, -2343 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { ONE, ONE, ONE },
        },
        .inner = 2000,
        .outer = 4500,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -6829, -6200, -5876 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { ONE, ONE, ONE },
        },
        .inner = 1500,
        .outer = 3000,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -4259, -6200, 2340 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { ONE, ONE, ONE },
        },
        .inner = 2000,
        .outer = 4500,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -4417, -2000, -1229 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { ONE, ONE, ONE },
        },
        .inner = 1500,
        .outer = 4000,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -6888, -5200, 9970 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { ONE, ONE, ONE },
        },
        .inner = 1000,
        .outer = 2500,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -6888, -5200, 8124 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { ONE, ONE, ONE },
        },
        .inner = 1000,
        .outer = 2500,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -878, -2000, -2607 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { 3686, ONE, 3686 },
        },
        .inner = 1500,
        .outer = 4000,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -878, -2000, 7297 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { 3686, ONE, 3686 },
        },
        .inner = 1500,
        .outer = 4000,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -4253, -2000, 0x2A75 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { ONE, ONE, ONE },
        },
        .inner = 1000,
        .outer = 2500,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -6862, -2000, 9652 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { ONE, ONE, ONE },
        },
        .inner = 1000,
        .outer = 2500,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -6862, -2000, 6623 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { ONE, ONE, ONE },
        },
        .inner = 1000,
        .outer = 2500,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -1664, -2000, 9386 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { ONE, ONE, ONE },
        },
        .inner = 1500,
        .outer = 4000,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -6862, -2000, -3140 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { ONE, ONE, ONE },
        },
        .inner = 1000,
        .outer = 2500,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { -3635, -6200, -5930 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { ONE, ONE, ONE },
        },
        .inner = 3000,
        .outer = 5500,
    },
};

WorldCoordRoomLights D_dryfield_night_motel_balcony_8018DA8C[1] = {
    { 0, NULL, ARRAY_SIZE(_gDryfieldNightMotelBalconyRooms1And3PointLights), _gDryfieldNightMotelBalconyRooms1And3PointLights, 0, NULL },
};

WorldCoordPointLight D_dryfield_night_motel_balcony_8018DAA4[22] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -4417, -2000, 2032 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 4096, 4096 }, { 0, 0 } }, 1500, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -6862, -2000, 316 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 4096, 4096 }, { 0, 0 } }, 500, 2000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -2611, -5200, 0x2A75 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 4096, 4096 }, { 0, 0 } }, 1000, 2500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -6888, -5200, 3833 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 4096, 4096 }, { 0, 0 } }, 1000, 2500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -6888, -5200, 5310 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 4096, 4096 }, { 0, 0 } }, 1000, 2500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -6888, -5200, -3212 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 4096, 4096 }, { 0, 0 } }, 1000, 2500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -0x2FD2, -5200, 424 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 4096, 4096 }, { 0, 0 } }, 1339, 4141 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -3755, -6200, 7089 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 4096, 4096 }, { 0, 0 } }, 2000, 8921 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -3635, -6200, -2343 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 4096, 4096 }, { 0, 0 } }, 2000, 7181 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -6829, -6200, -5876 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 4096, 4096 }, { 0, 0 } }, 1500, 3000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -4259, -6200, 2340 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 4096, 4096 }, { 0, 0 } }, 2000, 4500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -4417, -2000, -1229 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 4096, 4096 }, { 0, 0 } }, 1500, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -6888, -5200, 9970 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 4096, 4096 }, { 0, 0 } }, 1000, 2500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -6888, -5200, 8124 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 4096, 4096 }, { 0, 0 } }, 1000, 2500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -878, -2000, -2607 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3686, 4096, 3686 }, { 0, 0 } }, 1500, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -878, -2000, 7297 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3686, 4096, 3686 }, { 0, 0 } }, 1500, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -4253, -2000, 0x2A75 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 4096, 4096 }, { 0, 0 } }, 1000, 2500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -6862, -2000, 9652 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 4096, 4096 }, { 0, 0 } }, 1000, 2500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -6862, -2000, 6623 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 4096, 4096 }, { 0, 0 } }, 1000, 2500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -1664, -2000, 9386 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 4096, 4096 }, { 0, 0 } }, 1500, 4000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -6862, -2000, -3140 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 4096, 4096 }, { 0, 0 } }, 1000, 2500 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -3635, -6200, -5930 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 4096, 4096 }, { 0, 0 } }, 3000, 5500 },
};

WorldCoordRoomLights D_dryfield_night_motel_balcony_8018E2E4[1] = {
    { 0, NULL, ARRAY_SIZE(D_dryfield_night_motel_balcony_8018DAA4), D_dryfield_night_motel_balcony_8018DAA4, 0, NULL },
};

WorldCollisionTrigger D_dryfield_night_motel_balcony_8018E2FC[8] = {
    { NULL, NULL, NULL, { -6032, -4416, 320, 0 }, { { -1360, 3008, 0, 0 }, { 1360, 3008, 0, 0 }, { -1360, -3008, 0, 0 }, { 1360, -3008, 0, 0 } }, { 0, 0, 4100, 0 }, { 0, 0, 4096, 0 }, 3298, 0, 5, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -6176, -3968, 480, 0 }, { { 1360, 2272, 0, 0 }, { -1360, 2272, 0, 0 }, { 1360, -2272, 0, 0 }, { -1360, -2272, 0, 0 } }, { 0, 0, -4110, 0 }, { 0, 0, 4096, 0 }, 2635, 0, 6, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -7168, -4000, 1952, 0 }, { { 66, 2304, 1358, 0 }, { -67, 2304, -1359, 0 }, { 66, -2304, 1358, 0 }, { -67, -2304, -1359, 0 } }, { 4102, 0, -202, 0 }, { 0, 0, 4096, 0 }, 2672, 0, 5, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -7073, -4096, 1855, 0 }, { { -66, 2528, -1358, 0 }, { 67, 2528, 1359, 0 }, { -66, -2528, -1358, 0 }, { 67, -2528, 1359, 0 } }, { -4091, 0, 200, 0 }, { 0, 0, 4096, 0 }, 2862, 0, 3, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -6177, -4400, 6206, 0 }, { { -1323, 2576, 327, 0 }, { 1316, 2576, -332, 0 }, { -1323, -2576, 327, 0 }, { 1316, -2576, -332, 0 } }, { 992, 0, 3980, 0 }, { 0, 0, 4096, 0 }, 2907, 0, 4, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -6180, -4656, 6363, 0 }, { { 1320, 2704, -329, 0 }, { -1319, 2704, 330, 0 }, { 1320, -2704, -329, 0 }, { -1319, -2704, 330, 0 } }, { -996, 0, -3982, 0 }, { 0, 0, 4096, 0 }, 3018, 0, 5, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -0x2EB7, -4032, 2043, 0 }, { { 1, 2480, 1361, 0 }, { 0, 2480, -1361, 0 }, { 1, -2480, 1361, 0 }, { 0, -2480, -1361, 0 } }, { 4103, 0, -3, 0 }, { 0, 0, 4096, 0 }, 2827, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -0x2E15, -3936, 2013, 0 }, { { 0, 2480, -1361, 0 }, { 1, 2480, 1361, 0 }, { 0, -2480, -1361, 0 }, { 1, -2480, 1361, 0 } }, { -4106, 0, 1, 0 }, { 0, 0, 4096, 0 }, 2827, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionTrigger D_dryfield_night_motel_balcony_8018E55C[10] = {
    { NULL, NULL, NULL, { -6032, -4416, 320, 0 }, { { -1360, 3008, 0, 0 }, { 1360, 3008, 0, 0 }, { -1360, -3008, 0, 0 }, { 1360, -3008, 0, 0 } }, { 0, 0, 4100, 0 }, { 0, 0, 4096, 0 }, 3298, 0, 5, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -6176, -3968, 480, 0 }, { { 1360, 2272, 0, 0 }, { -1360, 2272, 0, 0 }, { 1360, -2272, 0, 0 }, { -1360, -2272, 0, 0 } }, { 0, 0, -4110, 0 }, { 0, 0, 4096, 0 }, 2635, 0, 6, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -7168, -4000, 1952, 0 }, { { 66, 2304, 1358, 0 }, { -67, 2304, -1359, 0 }, { 66, -2304, 1358, 0 }, { -67, -2304, -1359, 0 } }, { 4102, 0, -202, 0 }, { 0, 0, 4096, 0 }, 2672, 0, 5, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -7073, -4096, 1855, 0 }, { { -66, 2528, -1358, 0 }, { 67, 2528, 1359, 0 }, { -66, -2528, -1358, 0 }, { 67, -2528, 1359, 0 } }, { -4091, 0, 200, 0 }, { 0, 0, 4096, 0 }, 2862, 0, 3, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -6177, -4400, 6206, 0 }, { { -1323, 2576, 327, 0 }, { 1316, 2576, -332, 0 }, { -1323, -2576, 327, 0 }, { 1316, -2576, -332, 0 } }, { 992, 0, 3980, 0 }, { 0, 0, 4096, 0 }, 2907, 0, 4, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -6180, -4656, 6331, 0 }, { { 1320, 2704, -329, 0 }, { -1319, 2704, 330, 0 }, { 1320, -2704, -329, 0 }, { -1319, -2704, 330, 0 } }, { -996, 0, -3982, 0 }, { 0, 0, 4096, 0 }, 3018, 0, 5, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -9463, -4032, 2043, 0 }, { { -266, 2480, 1333, 0 }, { 263, 2480, -1337, 0 }, { -266, -2480, 1333, 0 }, { 263, -2480, -1337, 0 } }, { 4025, 0, 796, 0 }, { 0, 0, 4096, 0 }, 2827, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -9301, -3936, 2013, 0 }, { { 198, 2480, -1348, 0 }, { -200, 2480, 1345, 0 }, { 198, -2480, -1348, 0 }, { -200, -2480, 1345, 0 } }, { -4062, 0, -601, 0 }, { 0, 0, 4096, 0 }, 2827, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -5969, -4800, 0x2780, 0 }, { { 1196, 2480, -1184, 0 }, { -1195, 2480, 1184, 0 }, { 1196, -2480, -1184, 0 }, { -1195, -2480, 1184, 0 } }, { -2885, 0, -2913, 0 }, { 0, 0, 4096, 0 }, 2996, 0, 4, 9, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -6096, -4736, 9968, 0 }, { { -1156, 2480, 1136, 0 }, { 1157, 2480, -1136, 0 }, { -1156, -2480, 1136, 0 }, { 1157, -2480, -1136, 0 } }, { 2870, 0, 2921, 0 }, { 0, 0, 4096, 0 }, 2952, 0, 9, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionTrigger D_dryfield_night_motel_balcony_8018E854[6] = {
    { NULL, NULL, NULL, { -6032, -4416, 736, 0 }, { { -1360, 3008, 0, 0 }, { 1360, 3008, 0, 0 }, { -1360, -3008, 0, 0 }, { 1360, -3008, 0, 0 } }, { 0, 0, 4100, 0 }, { 0, 0, 4096, 0 }, 3298, 0, 3, 6, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -6176, -3968, 864, 0 }, { { 1360, 2272, 0, 0 }, { -1360, 2272, 0, 0 }, { 1360, -2272, 0, 0 }, { -1360, -2272, 0, 0 } }, { 0, 0, -4110, 0 }, { 0, 0, 4096, 0 }, 2635, 0, 6, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -6181, -4400, 8571, 0 }, { { -1360, 2576, -1, 0 }, { 1360, 2576, 1, 0 }, { -1360, -2576, -1, 0 }, { 1360, -2576, 1, 0 } }, { -4, 0, 4102, 0 }, { 0, 0, 4096, 0 }, 2907, 0, 4, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -6149, -4656, 8698, 0 }, { { 1361, 2704, -19, 0 }, { -1360, 2704, 20, 0 }, { 1361, -2704, -19, 0 }, { -1360, -2704, 20, 0 } }, { -60, 0, -4095, 0 }, { 0, 0, 4096, 0 }, 3018, 0, 5, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -9207, -4032, 2043, 0 }, { { -133, 2480, 1354, 0 }, { 132, 2480, -1356, 0 }, { -133, -2480, 1354, 0 }, { 132, -2480, -1356, 0 } }, { 4085, 0, 398, 0 }, { 0, 0, 4096, 0 }, 2827, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -9046, -3936, 2011, 0 }, { { 132, 2480, -1355, 0 }, { -133, 2480, 1355, 0 }, { 132, -2480, -1355, 0 }, { -133, -2480, 1355, 0 } }, { -4087, 0, -400, 0 }, { 0, 0, 4096, 0 }, 2827, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

AreaResource D_dryfield_night_motel_balcony_8018EA1C[2] = {
    { 16, 16, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, D_actor_101600_801445DC },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_dryfield_night_motel_balcony_8018EA34[4] = {
    { 31, 31, AREA_RESOURCE_FILE_GROUP_BASE_40, 0, { 0, 0 }, D_actor_403100_8015560C },
    { 106, 358, AREA_RESOURCE_FILE_GROUP_BASE_30, 0, { 0, 0 }, D_actor_335800_8016EADC },
    { 114, 358, AREA_RESOURCE_FILE_GROUP_BASE_60, 0, { 0, 0 }, &D_actor_335800_80172E9C },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_dryfield_night_motel_balcony_8018EA64[2] = {
    { 6, 6, AREA_RESOURCE_FILE_GROUP_BASE_40, 0, { 0, 0 }, &D_actor_400600_80151B10 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_dryfield_night_motel_balcony_8018EA7C[2] = {
    { 15, 15, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, &Actor01500_D0A008 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaVariant D_dryfield_night_motel_balcony_8018EA94[13] = {
    { NULL, NULL },
    { D_map_dryfield_full_8017CA28, D_dryfield_night_motel_balcony_8018EA1C },
    { D_map_dryfield_full_8017CA88, D_dryfield_night_motel_balcony_8018EA34 },
    { D_map_dryfield_full_8017CAC8, D_dryfield_night_motel_balcony_8018EA64 },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { D_map_dryfield_full_8017CAF8, D_dryfield_night_motel_balcony_8018EA7C },
    { NULL, NULL },
};

WorldCollisionTrigger D_dryfield_night_motel_balcony_8018EAFC[4] = {
    { NULL, NULL, NULL, { -0x3100, -3264, 1248, 0 }, { { -608, 0, -224, 0 }, { 608, 0, -224, 0 }, { -608, 0, 224, 0 }, { 608, 0, 224, 0 } }, { 0, 4098, 0, 0 }, { 401, 0, 4076, 0 }, 646, WORLD_COLLISION_TRIGGER_ACTION_WARP, 15, 21, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -6816, -3264, -2816, 0 }, { { 224, 0, -608, 0 }, { 224, 0, 608, 0 }, { -224, 0, -608, 0 }, { -224, 0, 608, 0 } }, { 0, 4098, 0, 0 }, { 4076, 0, -401, 0 }, 646, WORLD_COLLISION_TRIGGER_ACTION_WARP, 28, 33, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -6752, -3280, 4448, 0 }, { { 224, 0, -608, 0 }, { 224, 0, 608, 0 }, { -224, 0, -608, 0 }, { -224, 0, 608, 0 } }, { 0, 4098, 0, 0 }, { 4076, 0, -401, 0 }, 646, WORLD_COLLISION_TRIGGER_ACTION_WARP, 30, 49, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -1872, -3248, 0x2A2F, 0 }, { { 848, 0, 432, 0 }, { -848, 0, 432, 0 }, { 848, 0, -432, 0 }, { -848, 0, -432, 0 } }, { 0, 4102, 0, 0 }, { -51, 0, -4096, 0 }, 951, WORLD_COLLISION_TRIGGER_ACTION_WARP, 31, 65, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionTrigger D_dryfield_night_motel_balcony_8018EC2C[4] = {
    { NULL, NULL, NULL, { -0x3100, -3264, 1248, 0 }, { { -608, 0, -224, 0 }, { 608, 0, -224, 0 }, { -608, 0, 224, 0 }, { 608, 0, 224, 0 } }, { 0, 4098, 0, 0 }, { 401, 0, 4076, 0 }, 646, WORLD_COLLISION_TRIGGER_ACTION_CAP, 8, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -6816, -3264, -2816, 0 }, { { 224, 0, -608, 0 }, { 224, 0, 608, 0 }, { -224, 0, -608, 0 }, { -224, 0, 608, 0 } }, { 0, 4098, 0, 0 }, { 4076, 0, -401, 0 }, 646, WORLD_COLLISION_TRIGGER_ACTION_CAP, 8, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -1920, -3248, 0x2950, 0 }, { { 832, 0, 336, 0 }, { -832, 0, 336, 0 }, { 832, 0, -336, 0 }, { -832, 0, -336, 0 } }, { 0, 4117, 0, 0 }, { -51, 0, -4096, 0 }, 896, WORLD_COLLISION_TRIGGER_ACTION_CAP, 8, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -6784, -3254, 3872, 0 }, { { 224, 0, -608, 0 }, { 224, 0, 608, 0 }, { -224, 0, -608, 0 }, { -224, 0, 608, 0 } }, { 0, 4098, 0, 0 }, { 4076, 0, -401, 0 }, 646, WORLD_COLLISION_TRIGGER_ACTION_CAP, 8, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionTrigger D_dryfield_night_motel_balcony_8018ED5C[7] = {
    { NULL, NULL, NULL, { -0x3100, -3264, 1248, 0 }, { { -608, 0, -224, 0 }, { 608, 0, -224, 0 }, { -608, 0, 224, 0 }, { 608, 0, 224, 0 } }, { 0, 4098, 0, 0 }, { 401, 0, 4076, 0 }, 646, WORLD_COLLISION_TRIGGER_ACTION_WARP, 15, 21, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -6816, -3264, -2816, 0 }, { { 224, 0, -608, 0 }, { 224, 0, 608, 0 }, { -224, 0, -608, 0 }, { -224, 0, 608, 0 } }, { 0, 4098, 0, 0 }, { 4076, 0, -401, 0 }, 646, WORLD_COLLISION_TRIGGER_ACTION_WARP, 28, 33, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -6752, -3280, 4448, 0 }, { { 224, 0, -608, 0 }, { 224, 0, 608, 0 }, { -224, 0, -608, 0 }, { -224, 0, 608, 0 } }, { 0, 4098, 0, 0 }, { 4076, 0, -401, 0 }, 646, WORLD_COLLISION_TRIGGER_ACTION_WARP, 30, 49, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -1872, -3248, 0x2A2F, 0 }, { { 848, 0, 432, 0 }, { -848, 0, 432, 0 }, { 848, 0, -432, 0 }, { -848, 0, -432, 0 } }, { 0, 4102, 0, 0 }, { -51, 0, -4096, 0 }, 951, WORLD_COLLISION_TRIGGER_ACTION_WARP, 31, 65, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -6032, -3264, 4272, 0 }, { { -1232, 0, -848, 0 }, { 1232, 0, 80, 0 }, { -1232, 0, -80, 0 }, { 1232, 0, 848, 0 } }, { 0, 4100, 0, 0 }, { 401, 0, 4076, 0 }, 1492, WORLD_COLLISION_TRIGGER_ACTION_CAP, 9, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -7632, -3264, 2176, 0 }, { { -2688, 0, 560, 0 }, { 2688, 0, -1328, 0 }, { -2688, 0, 1328, 0 }, { 2688, 0, -560, 0 } }, { 0, 4095, 0, 0 }, { -3919, 0, -1189, 0 }, 2996, WORLD_COLLISION_TRIGGER_ACTION_CAP, 9, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -7648, -3264, 2176, 0 }, { { -2688, 0, 560, 0 }, { 2688, 0, -1328, 0 }, { -2688, 0, 1328, 0 }, { 2688, 0, -560, 0 } }, { 0, 4095, 0, 0 }, { 401, 0, -4076, 0 }, 2996, WORLD_COLLISION_TRIGGER_ACTION_CAP, 9, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionOccluder D_dryfield_night_motel_balcony_8018EF70[2] = {
    { NULL, NULL, { -0x2840, -4064, 7440, 0 }, { { -3104, -2624, 4336, 0 }, { 3104, -2624, -4336, 0 }, { -3104, 2624, 4336, 0 }, { 3104, 2624, -4336, 0 } }, { -3332, 0, -2385, 0 }, 5926, 1, 0 },
    { NULL, NULL, { -0x2B61, -4016, -2561, 0 }, { { 4011, -2640, 3514, 0 }, { -4010, -2640, -3513, 0 }, { 4011, 2640, 3514, 0 }, { -4010, 2640, -3513, 0 } }, { -2701, 0, 3082, 0 }, 5948, 1 | WORLD_COLLISION_OCCLUDER_LAST, 0 },
};

WorldCoordRoomAmbientEntry D_dryfield_night_motel_balcony_8018EFE8[40] = {
    { .viewCount = ARRAY_SIZE(D_dryfield_night_motel_balcony_8018EFE8) - 1 },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 618, 618, 618, 618 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 615, 618, 618, 616 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
};

WorldCoordRoomAmbientEntry D_dryfield_night_motel_balcony_8018F128[40] = {
    { .viewCount = ARRAY_SIZE(D_dryfield_night_motel_balcony_8018F128) - 1 },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 618, 618, 618, 618 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 615, 618, 618, 616 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 618, 618, 618, 618 } },
    { .color = { 618, 618, 618, 618 } },
    { .color = { 16, 16, 16, 16 } },
    { .color = { 16, 16, 16, 16 } },
};

WorldCollisionFootstepSounds D_dryfield_night_motel_balcony_8018F268 = {
    0x10000049,
    0x1000004B,
    0x10000049,
};

WorldCollisionFootstepSounds D_dryfield_night_motel_balcony_8018F274 = {
    0x1000002D,
    0x1000002F,
    0x1000002D,
};

WorldCollisionFootstepSounds D_dryfield_night_motel_balcony_8018F280 = {
    0x10000051,
    0x10000053,
    0x10000055,
};

WorldCollisionSurfaceProperties D_dryfield_night_motel_balcony_8018F28C[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_dryfield_night_motel_balcony_8018F280 },
};

WorldCollisionSurfaceProperties D_dryfield_night_motel_balcony_8018F294[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_dryfield_night_motel_balcony_8018F268 },
};

WorldCollisionSurfaceProperties D_dryfield_night_motel_balcony_8018F29C[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_dryfield_night_motel_balcony_8018F274 },
};

WorldCollisionSurfaceProperties D_dryfield_night_motel_balcony_8018F2A4[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_dryfield_night_motel_balcony_8018F280 },
};

WorldCollisionSurfaceProperties* D_dryfield_night_motel_balcony_8018F2AC[8] = {
    D_dryfield_night_motel_balcony_8018F28C,
    D_dryfield_night_motel_balcony_8018F294,
    D_dryfield_night_motel_balcony_8018F29C,
    D_dryfield_night_motel_balcony_8018F2A4,
    D_dryfield_night_motel_balcony_8018F28C,
    D_dryfield_night_motel_balcony_8018F28C,
    D_dryfield_night_motel_balcony_8018F28C,
    D_dryfield_night_motel_balcony_8018F28C,
};

/// The room's ambient effect task. Each tick it draws the glows whose bit for
/// the current view is set in the per-view mask table, turns two of them off
/// for good once flag nibble 0x7F is set (spawning effect 0x60094 the first
/// time), and runs the current view's timed effect bursts off the work
/// block's two counters.
void func_dryfield_night_motel_balcony_8017E554(Task* task)
{
    EffectWork* work;
    GfxCoord*   coord;
    s32         hi;
    s32         mask;
    s32         i;
    s32         n;
    s16         cnt;
    SVECTOR     pos;
    SVECTOR     ofs;

    work                                = task->spawnArg2.pointer;
    coord                               = task->extra.coordBody->coord;
    gRoomEffectState->groundShadowShade = ROOM_EFFECT_GROUND_SHADOW_MAX_SHADE;
    hi                                  = 0;
    if (gGameSession->location.loc.view < 0x20) {
        mask = 1 << gGameSession->location.loc.view;
    } else {
        mask = 1 << (gGameSession->location.loc.view - 0x20);
        hi   = 1;
    }
    if (mask & D_dryfield_night_motel_balcony_80182D40[hi][0]) {
        glowDrawShaft(&D_dryfield_night_motel_balcony_80182C60[0], 0x180);
    }
    if (mask & D_dryfield_night_motel_balcony_80182D40[hi][2]) {
        glowDrawShaft(&D_dryfield_night_motel_balcony_80182C70, 0x180);
    }
    if (mask & D_dryfield_night_motel_balcony_80182D40[hi][4]) {
        glowDrawShaft(&D_dryfield_night_motel_balcony_80182C80, 0x180);
    }
    if (mask & D_dryfield_night_motel_balcony_80182D40[hi][6]) {
        glowDrawShaft(&D_dryfield_night_motel_balcony_80182C90, 0x180);
    }
    if (mask & D_dryfield_night_motel_balcony_80182D40[hi][8]) {
        glowDrawShaft(&D_dryfield_night_motel_balcony_80182CA0, 0x180);
    }
    for (i = 10; i < 18; i++) {
        if (mask & D_dryfield_night_motel_balcony_80182D40[hi][i]) {
            glowDrawFlare(&D_dryfield_night_motel_balcony_80182C60[i], 1, 0x380);
        }
    }
    if (mask & D_dryfield_night_motel_balcony_80182D40[hi][18]) {
        glowDrawShaft(&D_dryfield_night_motel_balcony_80182CF0, 0x180);
    }
    if (gameFlagGetNibble(GAME_FLAG_07F) == 1) {
        D_dryfield_night_motel_balcony_80182D40[0][3] = 0;
        D_dryfield_night_motel_balcony_80182D40[0][2] = 0;
        D_dryfield_night_motel_balcony_80182D40[1][3] = 0;
        D_dryfield_night_motel_balcony_80182D40[1][2] = 0;
        Gp_SpawnEff(EFFECT_NIGHT_MOTEL_BALCONY_LAMP_BURST, coord, 0, &D_dryfield_night_motel_balcony_80182C70);
        gameFlagSetNibble(GAME_FLAG_07F, 2);
    } else if (gameFlagGetNibble(GAME_FLAG_07F) == 2) {
        D_dryfield_night_motel_balcony_80182D40[0][3] = 0;
        D_dryfield_night_motel_balcony_80182D40[0][2] = 0;
        D_dryfield_night_motel_balcony_80182D40[1][3] = 0;
        D_dryfield_night_motel_balcony_80182D40[1][2] = 0;
    }
    switch (gGameSession->location.loc.view) {
        case 17:
            if (++work->angle == 0x5C) {
                Gp_SpawnEff(EFFECT_DRYFIELD_NIGHT_MOTEL_DRIFT_PUFF, coord, 0x40000300, &D_dryfield_night_motel_balcony_80182D28);
                Gp_SpawnEff(EFFECT_DRYFIELD_NIGHT_MOTEL_DRIFT_PUFF, coord, 0x40000300, &D_dryfield_night_motel_balcony_80182D28);
                for (i = 0; i < 3; i++) {
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    Gp_SpawnEff(EFFECT_DRYFIELD_NIGHT_MOTEL_BALC_DEBRIS, coord, ((gRandomLcgState >> 16) & 0xFF) | 0x80010100,
                                &D_dryfield_night_motel_balcony_80182D28);
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    Gp_SpawnEff(EFFECT_DRYFIELD_NIGHT_MOTEL_BALC_DEBRIS, coord, ((gRandomLcgState >> 16) & 0x7F) | 0x80000080,
                                &D_dryfield_night_motel_balcony_80182D28);
                }
            }
            break;
        case 18:
            if (++work->scale == 0x3F) {
                Gp_SpawnEff(EFFECT_DRYFIELD_NIGHT_MOTEL_BALC_BREAK, coord, 3, &D_dryfield_night_motel_balcony_80182D08);
                work->angle = 0;
            }
            break;
        case 21:
            cnt = ++work->angle;
            if (cnt >= 0x47) {
                n = cnt - 0x46;
                if ((s16)(cnt % 6) == 0) {
                    memset(&ofs, 0, sizeof(ofs));
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    ofs.vx          = -((s32)(gRandomLcgState >> 16) % (n * 10));
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    ofs.vy          = (s32)(gRandomLcgState >> 16) % (n * 10);
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    ofs.vz          = (s32)(gRandomLcgState >> 16) % (n * 10);
                    pos             = ofs;
                    pos.vx         += D_dryfield_night_motel_balcony_80182D30.vx;
                    pos.vy         += D_dryfield_night_motel_balcony_80182D30.vy;
                    pos.vz         += D_dryfield_night_motel_balcony_80182D30.vz;
                    Gp_SpawnEff(EFFECT_NIGHT_MOTEL_BALCONY_FLAME, coord, n * 0x28 + 0x40000600, &pos);
                }
                work->scale = 0;
            }
            break;
        case 19:
            if (++work->scale == 0x2B) {
                Gp_SpawnEff(EFFECT_DRYFIELD_NIGHT_MOTEL_BALC_BREAK, coord, 4, &D_dryfield_night_motel_balcony_80182D10);
            }
            break;
        case 20:
            if (++work->scale == 0xC) {
                Gp_SpawnEff(EFFECT_DRYFIELD_NIGHT_MOTEL_BALC_BREAK, coord, 2, &D_dryfield_night_motel_balcony_80182D00);
            }
            break;
        case 23:
            if (++work->scale == 0xC) {
                Gp_SpawnEff(EFFECT_DRYFIELD_NIGHT_MOTEL_BALC_BREAK, coord, 2, &D_dryfield_night_motel_balcony_80182D38);
            }
            break;
        case 29:
            if (++work->scale == 0x41) {
                for (i = 0; i < 3; i++) {
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    Gp_SpawnEff(EFFECT_DRYFIELD_NIGHT_MOTEL_BALC_DEBRIS, coord, ((gRandomLcgState >> 16) & 0x7F) | 0x80000080,
                                &D_dryfield_night_motel_balcony_80182D18);
                }
            }
            break;
        default:
            work->scale = 0;
            work->angle = 0;
            break;
    }
}

#include "../../shared/glow_draw_shaft.inc.c"

#include "../../shared/glow_draw_flare.inc.c"

/// Draws one axis-aligned `POLY_FT4` panel of a 0x28-pixel sprite at the packed
/// screen position `arg0` (x in the low half, y in the high half). `arg1` is
/// the ordering-table index, `arg2` the panel width and `arg3` the animation
/// step, which walks frames 2..11 of `gEffectSpriteAtlasFrames`. The quad is `2 * d` wide and
/// `4 * d` tall, anchored three quarters of the way down, and both `d` and the
/// rounded weight `3 * d` are the one reused local the ROM keeps for them.
void func_dryfield_night_motel_balcony_8017F6C8(s32 arg0, s16 arg1, s16 arg2, s16 arg3)
{
    enum { FIRST_TEXTURE_FRAME = 2 };
    POLY_FT4*                       prim;
    const EffectSpriteTextureFrame* textureFrame;
    s16                             textureFrameIndex;
    const EffectSpriteTextureFrame* textureFrames;
    s32                             d;
    s32                             y;

    prim           = gGpuPrimCursor;
    gGpuPrimCursor = prim + 1;
    setlen(prim, 9);
    setcode(prim, 0x2F);
    prim->tpage = EFFECT_SPRITE_ATLAS_TEXTURE_PAGE;

    // Repeat the atlas tail, skipping the two opening frames.
    textureFrameIndex = arg3 % (ARRAY_SIZE(gEffectSpriteAtlasFrames) - FIRST_TEXTURE_FRAME) + FIRST_TEXTURE_FRAME;
    textureFrames     = gEffectSpriteAtlasFrames;
    textureFrame      = &textureFrames[textureFrameIndex];
    prim->clut        = getClut(textureFrame->clutX, textureFrame->clutY);
    prim->u0          = textureFrame->u;
    prim->v0          = textureFrame->v;
    prim->u1          = textureFrame->u + EFFECT_SPRITE_ATLAS_UV_SPAN;
    prim->v1          = textureFrame->v;
    prim->u2          = textureFrame->u;
    prim->v2          = textureFrame->v + EFFECT_SPRITE_ATLAS_UV_SPAN;
    prim->u3          = textureFrame->u + EFFECT_SPRITE_ATLAS_UV_SPAN;
    prim->v3          = textureFrame->v + EFFECT_SPRITE_ATLAS_UV_SPAN;

    d        = (arg2 * 0x1F) >> 12;
    prim->x2 = arg0 - d;
    prim->x0 = arg0 - d;
    prim->x3 = arg0 + d;
    prim->x1 = arg0 + d;

    d        = (arg2 * 0x1F) >> 13;
    y        = arg0 >> 16;
    prim->y1 = y - d * 3;
    prim->y0 = y - d * 3;
    prim->y3 = y + d;
    prim->y2 = y + d;

    addPrim(&gGpuCurrentOt[arg1], prim);
}

/// Per-frame handler of a falling room effect task that bounces. The first
/// frame resets the coordinate's rotation to identity, keeps the low twelve bits of
/// `Task::spawnArg1` in `pos.vx`, sets the speed `scale` to 0xA0, and rolls
/// a frame period (0..7) into `pos.vy`, a start frame into `index`, a value
/// into `pos.vz` and its per-tick step into `period`. When the spawner left
/// no drift it rolls one (negative `spawnArg1`: about +-0x40 across and
/// 0x20..0x11F in y; otherwise about +-0x80 on every axis) and turns it into
/// `parent`'s frame. `spawnArg1` is then replaced by two bits of its upper half.
/// Later frames step `pos.vz`, advance `index` once per period and move the
/// coordinate by the drift scaled to `scale`. When `worldCollisionProbeGridSegment` reports a hit
/// along the view-space step, the move is undone, the drift is bent halfway
/// towards the vector it returns, speed and step are halved and the coordinate moves
/// again; a hit within eight ticks of the previous one at a speed below 0x20
/// moves the task to state 2. Without a hit, `0xA000 / scale` is added to the
/// drift's y. Both states draw through
/// `func_dryfield_night_motel_balcony_8017FF78`, fading over ticks 60..89 and
/// releasing the task at 90. Event states 2 and 3 suspend it, 4 and above
/// release it at once, and event state 1 freezes the tick and the motion.
void func_dryfield_night_motel_balcony_8017F84C(Task* task)
{
    EffectWork* work  = task->spawnArg2.pointer;
    GfxCoord*   coord = task->extra.coordBody->coord;
    MATRIX*     m;
    s32         half;
    SVECTOR     delta;
    SVECTOR     dir;
    SVECTOR     pos;
    u8          color[3];

    if (gRoomEffectState->effectControl >= ROOM_EFFECT_CONTROL_HIDDEN) {
        if (gRoomEffectState->effectControl < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            return;
        }
        goto release;
    }

    actorRenderComposeCoord(coord);
    work->age++;

    switch (task->state) {
        case 0:
            m                    = &coord->coord;
            MATRIX_PAIR(m, 0, 0) = 0x1000;
            MATRIX_PAIR(m, 0, 2) = 0;
            MATRIX_PAIR(m, 1, 1) = 0x1000;
            MATRIX_PAIR(m, 2, 0) = 0;
            m->m[2][2]           = 0x1000;
            work->pos.vx         = (u16)task->spawnArg1.value & 0xFFF;
            work->scale          = 0xA0;
            gRandomLcgState      = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->pos.vy         = (gRandomLcgState >> 16) & 7;
            gRandomLcgState      = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->index          = (gRandomLcgState >> 16) & 7;
            gRandomLcgState      = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->pos.vz         = (gRandomLcgState >> 16) & 0xFFF;
            gRandomLcgState      = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->period         = 0x200 - ((gRandomLcgState >> 16) & 0x3FF);
            if ((work->move.vx | work->move.vy | work->move.vz) == 0) {
                if (task->spawnArg1.value < 0) {
                    half            = 0x40;
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    work->move.vx   = half - ((gRandomLcgState >> 16) & 0x7F);
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    work->move.vy   = ((gRandomLcgState >> 16) & 0xFF) + 0x20;
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    work->move.vz   = half - ((gRandomLcgState >> 16) & 0x7F);
                } else {
                    half            = 0x80;
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    work->move.vx   = half - ((gRandomLcgState >> 16) & 0xFF);
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    work->move.vy   = half - ((gRandomLcgState >> 16) & 0xFF);
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    work->move.vz   = half - ((gRandomLcgState >> 16) & 0xFF);
                }
                gte_SetRotMatrix(&work->parent->coord);
                gte_ldv0(&work->move);
                gte_rtv0();
                gte_stsv(&work->move);
            }
            VectorNormalSS(&work->move, &work->move);
            coord->composeStamp   = GRAPHICS_COORD_DIRTY;
            task->state           = 1;
            task->spawnArg1.value = (s16)(task->spawnArg1.value >> 16) & 3;
            break;
        case 1:
            if (gRoomEffectState->effectControl == ROOM_EFFECT_CONTROL_RUNNING) {
                work->pos.vz += work->period;
                if (work->pos.vy != 0 && work->age % work->pos.vy == 0) {
                    work->index++;
                }
                gte_lddp(work->scale);
                gte_ldsv(&work->move);
                gte_gpf12();
                gte_stsv(&delta);
                coord->coord.t[0]  += delta.vx;
                coord->coord.t[1]  += delta.vy;
                coord->coord.t[2]  += delta.vz;
                coord->composeStamp = GRAPHICS_COORD_DIRTY;
                gte_SetRotMatrix(&gGfxViewCoord.workm);
                gte_ldv0(&delta);
                gte_rtv0();
                gte_stsv(&dir);
                pos.vx  = coord->workm.t[0];
                pos.vy  = coord->workm.t[1];
                pos.vz  = coord->workm.t[2];
                dir.vx += pos.vx;
                dir.vy += pos.vy;
                dir.vz += pos.vz;
                if (worldCollisionProbeGridSegment(&dir, &pos, &dir, &pos) == 1) {
                    coord->coord.t[0] -= delta.vx;
                    coord->coord.t[1] -= delta.vy;
                    coord->coord.t[2] -= delta.vz;
                    work->move.vx      = (pos.vx >> 1) + (work->move.vx >> 1);
                    work->move.vy      = pos.vy + (work->move.vy >> 1);
                    work->move.vz      = (pos.vz >> 1) + (work->move.vz >> 1);
                    VectorNormalSS(&work->move, &work->move);
                    work->scale  = work->scale >> 1;
                    work->period = work->period >> 1;
                    gte_lddp(work->scale);
                    gte_ldsv(&work->move);
                    gte_gpf12();
                    gte_stsv(&delta);
                    coord->coord.t[0] += delta.vx;
                    coord->coord.t[1] += delta.vy;
                    coord->coord.t[2] += delta.vz;
                    if (work->age - work->step < 8 && work->scale < 0x20) {
                        task->state = 2;
                    } else {
                        work->step = work->age;
                    }
                } else if (work->scale > 0) {
                    work->move.vy += 0xA000 / work->scale;
                }
            } else {
                work->age--;
            }
            if (work->age < 60) {
                func_dryfield_night_motel_balcony_8017FF78(task, NULL, task->spawnArg1.value);
            } else if (work->age < 90) {
                color[0] = color[1] = color[2] = (90 - work->age) * 4;
                func_dryfield_night_motel_balcony_8017FF78(task, color, task->spawnArg1.value);
            } else {
                effectKillTask(work, task);
            }
            break;
        case 2:
            if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
                work->age--;
            }
            if (work->age < 60) {
                func_dryfield_night_motel_balcony_8017FF78(task, NULL, task->spawnArg1.value);
            } else if (work->age < 90) {
                color[0] = color[1] = color[2] = (90 - work->age) * 4;
                func_dryfield_night_motel_balcony_8017FF78(task, color, task->spawnArg1.value);
            } else {
            release:
                effectKillTask(work, task);
            }
            break;
    }
}

/// Draws the task's coordinate-body position as a rotated billboard `POLY_FT4`, taking
/// its texture frame from texture row `Task::spawnArg1` and column
/// `index & 7`. The quad's half-extent is the inclusive frame UV span times `pos.vx`
/// divided by the projected depth, rotated by `pos.vz`. A non-NULL `color`
/// tints the quad and makes it semi-transparent; NULL draws it raw. `arg` is
/// unused.
static void func_dryfield_night_motel_balcony_8017FF78(Task* task, u8* color, s32 arg)
{
    enum {
        DRYFIELD_NIGHT_MOTEL_BALCONY_DEBRIS_FRAMES_PER_ROW = 8,
        DRYFIELD_NIGHT_MOTEL_BALCONY_DEBRIS_CLUT_WORDS     = 16,
        DRYFIELD_NIGHT_MOTEL_BALCONY_DEBRIS_CLUT_Y         = 271,
    };
    EffectWork*         work  = task->spawnArg2.pointer;
    GfxCoord*           coord = task->extra.coordBody->coord;
    EffectShapeScratch* block;
    POLY_FT4*           prim;
    s16                 uvSpan;

    uvSpan = D_dryfield_night_motel_balcony_80182DE0[task->spawnArg1.value].frameSize - 1;
    SCRATCH_STACK_RESERVE_BLOCK(EffectShapeScratch);
    block                = SCRATCH_STACK_CURSOR(EffectShapeScratch);
    block->worldPoint.vx = coord->workm.t[0];
    block->worldPoint.vy = coord->workm.t[1];
    block->worldPoint.vz = coord->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->worldPoint);
    gte_rtps();
    gte_stsxy(&block->screenX);
    gte_stflg(&block->projectionFlags);
    if (block->projectionFlags >= 0) {
        gte_stszotz(&block->depth);
        prim           = gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2C);
        if (color != NULL) {
            prim->r0 = color[0];
            prim->g0 = color[1];
            prim->b0 = color[2];
            setSemiTrans(prim, 1);
        } else {
            setcode(prim, 0x2D);
        }
        prim->tpage            = getTPage(0, GPU_BLEND_ADD, D_dryfield_night_motel_balcony_80182DE0[task->spawnArg1.value].vramX, 0);
        prim->clut             = getClut(task->spawnArg1.value * DRYFIELD_NIGHT_MOTEL_BALCONY_DEBRIS_CLUT_WORDS, DRYFIELD_NIGHT_MOTEL_BALCONY_DEBRIS_CLUT_Y);
        prim->u0               = (work->index & (DRYFIELD_NIGHT_MOTEL_BALCONY_DEBRIS_FRAMES_PER_ROW - 1)) * D_dryfield_night_motel_balcony_80182DE0[task->spawnArg1.value].frameSize;
        prim->v0               = D_dryfield_night_motel_balcony_80182DE0[task->spawnArg1.value].v;
        prim->u1               = (work->index & (DRYFIELD_NIGHT_MOTEL_BALCONY_DEBRIS_FRAMES_PER_ROW - 1)) * D_dryfield_night_motel_balcony_80182DE0[task->spawnArg1.value].frameSize + uvSpan;
        prim->v1               = D_dryfield_night_motel_balcony_80182DE0[task->spawnArg1.value].v;
        prim->u2               = (work->index & (DRYFIELD_NIGHT_MOTEL_BALCONY_DEBRIS_FRAMES_PER_ROW - 1)) * D_dryfield_night_motel_balcony_80182DE0[task->spawnArg1.value].frameSize;
        prim->v2               = D_dryfield_night_motel_balcony_80182DE0[task->spawnArg1.value].v + uvSpan;
        prim->u3               = (work->index & (DRYFIELD_NIGHT_MOTEL_BALCONY_DEBRIS_FRAMES_PER_ROW - 1)) * D_dryfield_night_motel_balcony_80182DE0[task->spawnArg1.value].frameSize + uvSpan;
        prim->v3               = D_dryfield_night_motel_balcony_80182DE0[task->spawnArg1.value].v + uvSpan;
        block->extent.corner.x = (((uvSpan * work->pos.vx) / block->depth) * rsin(work->pos.vz)) >> 12;
        block->extent.corner.y = (((uvSpan * work->pos.vx) / block->depth) * rcos(work->pos.vz)) >> 12;
        prim->x0               = block->screenX + block->extent.corner.x;
        prim->x3               = block->screenX - block->extent.corner.x;
        prim->y0               = block->screenY - block->extent.corner.y;
        prim->y3               = block->screenY + block->extent.corner.y;
        block->extent.corner.x = (((uvSpan * work->pos.vx) / block->depth) * rsin(work->pos.vz + 0x400)) >> 12;
        block->extent.corner.y = (((uvSpan * work->pos.vx) / block->depth) * rcos(work->pos.vz + 0x400)) >> 12;
        prim->x1               = block->screenX + block->extent.corner.x;
        prim->x2               = block->screenX - block->extent.corner.x;
        prim->y1               = block->screenY - block->extent.corner.y;
        prim->y2               = block->screenY + block->extent.corner.y;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)), prim);
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectShapeScratch);
}

void func_dryfield_night_motel_balcony_80180580(Task* task)
{
    void*     work  = task->spawnArg2.pointer;
    GfxCoord* coord = task->extra.coordBody->coord;
    s32       i;

    if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
        if (gRoomEffectState->effectControl < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            return;
        }
        goto release;
    }
    switch (task->state) {
        case 0:
            task->state = task->spawnArg1.value * 2 + 1;
            break;
        case 1:
            for (i = 0; i < 8; i++) {
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                Gp_SpawnEff(EFFECT_DRYFIELD_NIGHT_MOTEL_BALC_DEBRIS, coord, ((gRandomLcgState >> 16) & 0xFF) | 0x100, NULL);
            }
            task->state = 2;
            break;
        case 2:
            for (i = 0; i < 4; i++) {
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                Gp_SpawnEff(EFFECT_DRYFIELD_NIGHT_MOTEL_BALC_DEBRIS, coord, ((gRandomLcgState >> 16) & 0xFF) | 0x10100, NULL);
                Gp_SpawnEff(EFFECT_DRYFIELD_NIGHT_MOTEL_DRIFT_PUFF, coord, 0x400, NULL);
            }
            task->state = 10;
            break;
        case 3:
            for (i = 0; i < 6; i++) {
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                Gp_SpawnEff(EFFECT_DRYFIELD_NIGHT_MOTEL_BALC_DEBRIS, coord, ((gRandomLcgState >> 16) & 0x7F) | 0x80, NULL);
            }
            task->state = 4;
            break;
        case 4:
            for (i = 0; i < 3; i++) {
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                Gp_SpawnEff(EFFECT_DRYFIELD_NIGHT_MOTEL_BALC_DEBRIS, coord, ((gRandomLcgState >> 16) & 0x7F) | 0x10080, NULL);
                Gp_SpawnEff(EFFECT_DRYFIELD_NIGHT_MOTEL_DRIFT_PUFF, coord, 0x400, NULL);
            }
            task->state = 10;
            break;
        case 5:
            for (i = 0; i < 4; i++) {
                Gp_SpawnEff(EFFECT_DRYFIELD_NIGHT_MOTEL_DRIFT_PUFF, coord, 0x40000300, NULL);
            }
            task->state = 6;
            break;
        case 6:
            for (i = 0; i < 4; i++) {
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                Gp_SpawnEff(EFFECT_DRYFIELD_NIGHT_MOTEL_BALC_DEBRIS, coord, ((gRandomLcgState >> 16) & 0x7F) | 0x80000080, NULL);
            }
            for (i = 0; i < 2; i++) {
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                Gp_SpawnEff(EFFECT_DRYFIELD_NIGHT_MOTEL_BALC_DEBRIS, coord, ((gRandomLcgState >> 16) & 0x7F) | 0x80010080, NULL);
            }
            task->state = 10;
            break;
        case 7:
            for (i = 0; i < 8; i++) {
                Gp_SpawnEff(EFFECT_DRYFIELD_NIGHT_MOTEL_DRIFT_PUFF, coord, 0x10400, NULL);
            }
            task->state = 8;
            break;
        case 8:
            for (i = 0; i < 8; i++) {
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                Gp_SpawnEff(EFFECT_DRYFIELD_NIGHT_MOTEL_BALC_DEBRIS, coord, ((gRandomLcgState >> 16) & 0xFF) | 0x100, NULL);
            }
            task->state = 10;
            break;
        case 9:
            for (i = 0; i < 8; i++) {
                Gp_SpawnEff(EFFECT_DRYFIELD_NIGHT_MOTEL_DRIFT_PUFF, coord, 0x10400, NULL);
            }
            task->state = 10;
            break;
        case 10:
        release:
            effectKillTask(work, task);
            break;
    }
}

void func_dryfield_night_motel_balcony_801809CC(Task* task)
{
    EffectWork*       work;
    GfxCoord*         coord;
    GfxRotationWords* rot;
    s16               flag;
    u16               age;
    s16               t;
    u8                color[3];

    work  = task->spawnArg2.pointer;
    flag  = gRoomEffectState->effectControl;
    coord = task->extra.coordBody->coord;
    if (flag >= ROOM_EFFECT_CONTROL_HIDDEN) {
        if (flag >= ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            effectKillTask(work, task);
        }
        return;
    }
    actorRenderComposeCoord(coord);
    age       = work->age;
    work->age = age + 1;
    switch (task->state) {
        case 0:
            rot             = (GfxRotationWords*)&coord->coord;
            rot->m00M01     = ONE;
            rot->m02M10     = 0;
            rot->m11M12     = ONE;
            rot->m20M21     = 0;
            rot->m22        = ONE;
            work->pos.vx    = task->spawnArg1.halves.low & 0xFFF;
            work->scale     = 0xA0;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->index     = (gRandomLcgState >> 16) & 7;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->move.vy   = (gRandomLcgState >> 16) & 0xFF;
            task->state     = 1;
            break;
        case 1:
            if (gRoomEffectState->effectControl == ROOM_EFFECT_CONTROL_RUNNING) {
                work->index++;
                coord->coord.t[1]  += work->move.vy;
                coord->composeStamp = GRAPHICS_COORD_DIRTY;
                if (coord->coord.t[1] > 0) {
                    if (work->age < 0x1E) {
                        Gp_SpawnEff(EFFECT_DRYFIELD_NIGHT_MOTEL_DRIFT_PUFF, coord, work->pos.vx + 0x20010400, NULL);
                    }
                    task->state = 2;
                } else if (work->scale > 0) {
                    work->move.vy += 6;
                }
            } else {
                work->age = age;
            }
            t = work->age;
            if (t < 0x14) {
                func_dryfield_night_motel_balcony_80180C60(task, NULL, 0);
            } else if (t < 0x1E) {
                color[0] = color[1] = color[2] = (0x1E - t) * 0xC;
                func_dryfield_night_motel_balcony_80180C60(task, color, 0);
            } else {
                effectKillTask(work, task);
            }
            break;
        case 2:
            if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
                work->age = age;
            }
            t = work->age;
            if (t < 0x14) {
                func_dryfield_night_motel_balcony_80180C60(task, NULL, 0);
            } else if (t < 0x1E) {
                color[0] = color[1] = color[2] = (0x1E - t) * 0xC;
                func_dryfield_night_motel_balcony_80180C60(task, color, 0);
            } else {
                effectKillTask(work, task);
            }
            break;
    }
}

/// Projects the task model's world position through `GsWSMATRIX` and, when the
/// GTE flag is non-negative, queues one `POLY_FT4` billboard (tpage 0x2C, clut
/// 0x43C3) centred on it. `index % 6` picks one of six 40-texel columns at
/// v 0x40..0x67, and the half-extent is `pos.vx * 39 / depth` on both axes.
/// `color` modulates the texture and makes the quad semi-transparent; NULL
/// draws the texture raw and opaque. The third argument is never read; every
/// caller passes 0.
static void func_dryfield_night_motel_balcony_80180C60(Task* task, u8* color, s32 unused)
{
    EffectWork*          work;
    GfxCoord*            coord;
    EffectCentreScratch* block;
    POLY_FT4*            prim;
    DisplayState*        ds;
    s16                  xy;

    coord = task->extra.coordBody->coord;
    work  = task->spawnArg2.pointer;

    SCRATCH_STACK_RESERVE_BLOCK(EffectCentreScratch);
    block                = SCRATCH_STACK_CURSOR(EffectCentreScratch);
    block->worldPoint.vx = coord->workm.t[0];
    block->worldPoint.vy = coord->workm.t[1];
    block->worldPoint.vz = coord->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->worldPoint);
    gte_rtps();
    gte_stsxy(&block->screenX);
    gte_stflg(&block->projectionFlags);
    if (block->projectionFlags >= 0) {
        gte_stszotz(&block->depth);
        prim           = gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2C);
        if (color != NULL) {
            prim->r0 = color[0];
            prim->g0 = color[1];
            prim->b0 = color[2];
            setSemiTrans(prim, 1);
        } else {
            setcode(prim, 0x2D);
        }
        prim->tpage         = 0x2C;
        prim->clut          = 0x43C3;
        prim->u0            = work->index % 6 * 40;
        prim->v0            = 0x40;
        prim->u1            = work->index % 6 * 40 + 0x27;
        prim->v1            = 0x40;
        prim->u2            = work->index % 6 * 40;
        prim->v2            = 0x67;
        prim->u3            = work->index % 6 * 40 + 0x27;
        prim->v3            = 0x67;
        block->screenExtent = work->pos.vx * 39 / block->depth;
        xy                  = block->screenX - block->screenExtent;
        prim->x2            = xy;
        prim->x0            = xy;
        xy                  = block->screenX + block->screenExtent;
        prim->x3            = xy;
        prim->x1            = xy;
        xy                  = block->screenY - block->screenExtent;
        prim->y1            = xy;
        prim->y0            = xy;
        xy                  = block->screenY + block->screenExtent;
        prim->y3            = xy;
        prim->y2            = xy;
        ds                  = &gDisplayState;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->depth << ds->otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                prim);
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectCentreScratch);
}

/// Per-frame handler of an effect-spawning room task. Any non-zero event state
/// suspends it, and 4 or above releases it. In view 0x27 it makes three
/// independent LCG rolls each frame: 1 in 4 spawns effect 0x6003D and 1 in 3
/// spawns 0x60093, both with an offset of up to 0x100 on every axis, and 1 in 7
/// spawns 0x60095 with a horizontal offset of up to 0x80. In any other view it
/// counts `age` up to 150 frames and then releases itself. Until then it
/// makes two rolls that fire less often as the count grows (the count must be
/// below a draw modulo 150, then modulo 120), each followed by a 1-in-4 roll
/// that spawns 0x60095, first with a vertical offset of up to 0x7FF and then at
/// a fixed height of 0xC00.
void func_dryfield_night_motel_balcony_80181024(Task* task)
{
    EffectWork* work  = task->spawnArg2.pointer;
    GfxCoord*   coord = task->extra.coordBody->coord;
    s32         lo;
    s32         arg;

    if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
        if (gRoomEffectState->effectControl < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            return;
        }
        goto release;
    }
    if (gGameSession->location.loc.view == 0x27) {
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        if (((gRandomLcgState >> 16) & 3) == 0) {
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->move.vx   = 0x100 - ((gRandomLcgState >> 16) & 0x1FF);
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->move.vy   = 0x100 - ((gRandomLcgState >> 16) & 0x1FF);
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->move.vz   = 0x100 - ((gRandomLcgState >> 16) & 0x1FF);
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            lo              = (gRandomLcgState >> 16) & 0x1FF;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            arg             = (((gRandomLcgState >> 16) % 3) << 16) + 0x80000100;
            Gp_SpawnEff(EFFECT_DRYFIELD_NIGHT_MOTEL_BALC_DEBRIS, coord, lo + arg, &work->move);
        }
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        if ((u16)((gRandomLcgState >> 16) % 3U) == 0) {
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->move.vx   = 0x100 - ((gRandomLcgState >> 16) & 0x1FF);
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->move.vy   = 0x100 - ((gRandomLcgState >> 16) & 0x1FF);
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->move.vz   = 0x100 - ((gRandomLcgState >> 16) & 0x1FF);
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            Gp_SpawnEff(EFFECT_DRYFIELD_NIGHT_MOTEL_BALC_FALLING, coord, ((gRandomLcgState >> 16) & 0x1FF) + 0x100, &work->move);
        }
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        if ((u16)((gRandomLcgState >> 16) % 7U) == 0) {
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->move.vx   = 0x80 - ((gRandomLcgState >> 16) & 0xFF);
            work->move.vy   = 0;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->move.vz   = 0x80 - ((gRandomLcgState >> 16) & 0xFF);
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            Gp_SpawnEff(EFFECT_DRYFIELD_NIGHT_MOTEL_DRIFT_PUFF, coord, ((gRandomLcgState >> 16) & 0x1FF) | 0xA0000400, &work->move);
        }
    } else {
        work->age++;
        if (work->age < 150) {
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            if (work->age < (u16)((gRandomLcgState >> 16) % 150U)) {
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                if (((gRandomLcgState >> 16) & 3) == 0) {
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    work->move.vx   = 0x80 - ((gRandomLcgState >> 16) & 0xFF);
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    work->move.vy   = (gRandomLcgState >> 16) & 0x7FF;
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    work->move.vz   = 0x80 - ((gRandomLcgState >> 16) & 0xFF);
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    Gp_SpawnEff(EFFECT_DRYFIELD_NIGHT_MOTEL_DRIFT_PUFF, coord, ((gRandomLcgState >> 16) & 0x1FF) | 0x80000400,
                                &work->move);
                }
            }
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            if (work->age < (u16)((gRandomLcgState >> 16) % 120U)) {
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                if (((gRandomLcgState >> 16) & 3) == 0) {
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    work->move.vx   = 0x100 - ((gRandomLcgState >> 16) & 0x1FF);
                    work->move.vy   = 0xC00;
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    work->move.vz   = 0x100 - ((gRandomLcgState >> 16) & 0x1FF);
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    Gp_SpawnEff(EFFECT_DRYFIELD_NIGHT_MOTEL_DRIFT_PUFF, coord, ((gRandomLcgState >> 16) & 0x1FF) | 0x20010400,
                                &work->move);
                }
            }
        } else {
        release:
            effectKillTask(work, task);
        }
    }
}

/// Per-frame handler of a drifting room effect task, a variant of
/// `func_dryfield_night_motel_balcony_80181E7C`. The first frame resets the
/// coordinate's rotation to identity, keeps the low twelve bits of
/// `Task::spawnArg1` in `pos.vx`, rolls a frame period (1..4 ticks) into
/// `pos.vy` and a value into `pos.vz`, and, when the spawner left no drift,
/// rolls one whose ranges depend on `spawnArg1` (bit 30: +-0x80 on every axis;
/// negative: +-0x10 across and 0..-0xFF in y; otherwise +-0x80 across and
/// 0..15 in y) and turns it into `parent`'s frame. The drift is normalised and scaled to `scale`
/// (0x40 with bit 30 or bit 29, else 0x80), and `spawnArg1` is replaced by
/// two bits of its upper half. Later frames advance `index` once per period,
/// move the coordinate by the drift, decrementing its y by one a tick, and hand
/// the task to `func_dryfield_night_motel_balcony_801819E0` until `index`
/// reaches 12, when it is released. Event states 2 and 3 suspend it, 4 and
/// above release it at once, and state 1 freezes the drift and the tick.
void func_dryfield_night_motel_balcony_8018158C(Task* task)
{
    EffectWork* work  = task->spawnArg2.pointer;
    GfxCoord*   coord = task->extra.coordBody->coord;
    MATRIX*     m;
    s32         half; // default drift length and the centre of the wide drift rolls

    if (gRoomEffectState->effectControl >= ROOM_EFFECT_CONTROL_HIDDEN) {
        if (gRoomEffectState->effectControl < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            return;
        }
        goto release;
    }

    actorRenderComposeCoord(coord);
    half = 0x80;
    work->age++;

    switch (task->state) {
        case 0:
            m                    = &coord->coord;
            MATRIX_PAIR(m, 0, 0) = 0x1000;
            MATRIX_PAIR(m, 0, 2) = 0;
            MATRIX_PAIR(m, 1, 1) = 0x1000;
            MATRIX_PAIR(m, 2, 0) = 0;
            m->m[2][2]           = 0x1000;
            work->pos.vx         = (u16)task->spawnArg1.value & 0xFFF;
            work->scale          = half;
            gRandomLcgState      = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->pos.vy         = ((gRandomLcgState >> 16) & 3) + 1;
            gRandomLcgState      = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->pos.vz         = (gRandomLcgState >> 16) & 0xFFF;
            work->index          = 0;
            if ((work->move.vx | work->move.vy | work->move.vz) == 0) {
                if (task->spawnArg1.value & 0x40000000) {
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    work->move.vx   = half - ((gRandomLcgState >> 16) & 0xFF);
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    work->move.vy   = half - ((gRandomLcgState >> 16) & 0xFF);
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    work->move.vz   = half - ((gRandomLcgState >> 16) & 0xFF);
                    work->scale     = 0x40;
                } else {
                    if (task->spawnArg1.value < 0) {
                        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        work->move.vx   = 0x10 - ((gRandomLcgState >> 16) & 0x1F);
                        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        work->move.vy   = -((gRandomLcgState >> 16) & 0xFF);
                        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        work->move.vz   = 0x10 - ((gRandomLcgState >> 16) & 0x1F);
                    } else {
                        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        work->move.vx   = half - ((gRandomLcgState >> 16) & 0xFF);
                        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        work->move.vy   = (gRandomLcgState >> 16) & 0xF;
                        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                        work->move.vz   = half - ((gRandomLcgState >> 16) & 0xFF);
                    }
                    if (task->spawnArg1.value & 0x20000000) {
                        work->scale = 0x40;
                    }
                }
                gte_SetRotMatrix(&work->parent->coord);
                gte_ldv0(&work->move);
                gte_rtv0();
                gte_stsv(&work->move);
            }
            VectorNormalSS(&work->move, &work->move);
            gte_lddp(work->scale);
            gte_ldsv(&work->move);
            gte_gpf12();
            gte_stsv(&work->move);
            coord->composeStamp   = GRAPHICS_COORD_DIRTY;
            task->state           = 1;
            task->spawnArg1.value = (s16)(task->spawnArg1.value >> 16) & 3;
            break;
        case 1:
            if (gRoomEffectState->effectControl == ROOM_EFFECT_CONTROL_RUNNING) {
                if (work->age % work->pos.vy == 0) {
                    work->index++;
                }
                coord->coord.t[0]  += work->move.vx;
                coord->coord.t[1]  += work->move.vy;
                coord->coord.t[2]  += work->move.vz;
                coord->composeStamp = GRAPHICS_COORD_DIRTY;
                work->move.vy--;
            } else {
                work->age--;
            }
            if (work->index < 12) {
                func_dryfield_night_motel_balcony_801819E0(task, task->spawnArg1.value);
            } else {
            release:
                effectKillTask(work, task);
            }
            break;
    }
}

/// Projects the task model's world position through `GsWSMATRIX` and, when the
/// GTE flag is non-negative, queues one semi-transparent `POLY_FT4` billboard
/// on tpage 0x2C centred on it. `index` is the animation frame: it picks a
/// 48-texel cell of a five-column sheet starting at v 0x68, and steps the CLUT
/// x by 16 per frame from the origin `arg` selects in
/// `D_dryfield_night_motel_balcony_80182DF4`. The half-extent is
/// `pos.vx * 47 / (depth + 1)` on both axes.
static void func_dryfield_night_motel_balcony_801819E0(Task* task, s32 arg)
{
    EffectWork*          work;
    GfxCoord*            coord;
    u8*                  head;
    EffectCentreScratch* block;
    POLY_FT4*            prim;
    DisplayState*        ds;
    _ClutOrigin*         clut;
    SVECTOR*             vec;
    s16                  xy;
    u16                  vz;

    coord = task->extra.coordBody->coord;
    work  = task->spawnArg2.pointer;

    head                                                                        = SCRATCH_STACK_CURSOR(void);
    ((EffectCentreScratch*)(head - sizeof(EffectCentreScratch)))->worldPoint.vx = (u16)coord->workm.t[0];
    block                                                                       = (EffectCentreScratch*)(head - sizeof(EffectCentreScratch));
    block->worldPoint.vy                                                        = (u16)coord->workm.t[1];
    vz                                                                          = (u16)coord->workm.t[2];
    SCRATCH_STACK_CURSOR(void)                                                  = block;
    block->worldPoint.vz                                                        = vz;
    vec                                                                         = &block->worldPoint;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(vec);
    gte_rtps();
    gte_stsxy(&((EffectCentreScratch*)(head - sizeof(EffectCentreScratch)))->screenX);
    gte_stflg(&((EffectCentreScratch*)(head - sizeof(EffectCentreScratch)))->projectionFlags);
    if (block->projectionFlags >= 0) {
        gte_stszotz(&((EffectCentreScratch*)(head - sizeof(EffectCentreScratch)))->depth);
        block->depth++;
        prim           = gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2F);
        prim->tpage         = 0x2C;
        clut                = &D_dryfield_night_motel_balcony_80182DF4[arg];
        prim->clut          = getClut(clut->x + work->index * 16, clut->y);
        prim->u0            = work->index % 5 * 48;
        prim->v0            = work->index / 5 * 48 + 0x68;
        prim->u1            = work->index % 5 * 48 + 0x2F;
        prim->v1            = work->index / 5 * 48 + 0x68;
        prim->u2            = work->index % 5 * 48;
        prim->v2            = work->index / 5 * 48 + 0x97;
        prim->u3            = work->index % 5 * 48 + 0x2F;
        prim->v3            = work->index / 5 * 48 + 0x97;
        block->screenExtent = work->pos.vx * 0x2F / block->depth;
        xy                  = block->screenX - (u16)block->screenExtent;
        prim->x2            = xy;
        prim->x0            = xy;
        xy                  = block->screenX + (u16)block->screenExtent;
        prim->x3            = xy;
        prim->x1            = xy;
        xy                  = block->screenY - (u16)block->screenExtent;
        prim->y1            = xy;
        prim->y0            = xy;
        xy                  = block->screenY + (u16)block->screenExtent;
        prim->y3            = xy;
        prim->y2            = xy;
        ds                  = &gDisplayState;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->depth << ds->otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                prim);
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectCentreScratch);
}

/// Per-frame handler of a drifting room effect task. The first frame resets the
/// coordinate's rotation to identity, rolls a starting animation step (0..9) and a
/// lifetime (5..14 ticks), and, when the spawner left no drift, rolls one and
/// turns it into `parent`'s frame. The drift is then normalised and scaled to a
/// length chosen by `Task::spawnArg1` (8 when negative, 0x80 with bit 30, 0x20
/// otherwise). Later frames move the coordinate by the drift, bending it by one
/// unit a tick, and draw it through `func_dryfield_night_motel_balcony_8018221C`,
/// fading its colour over the last ten ticks before releasing the task. Event
/// states 2 and 3 suspend it; 4 and above release it at once, and any non-zero
/// state below that freezes the drift and the lifetime tick.
void func_dryfield_night_motel_balcony_80181E7C(Task* task)
{
    EffectWork* work  = task->spawnArg2.pointer;
    GfxCoord*   coord = task->extra.coordBody->coord;
    MATRIX*     m;
    s32         seed;
    s16         tick;
    s16         end;
    u8          color[3];

    if (gRoomEffectState->effectControl >= ROOM_EFFECT_CONTROL_HIDDEN) {
        if (gRoomEffectState->effectControl < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            return;
        }
        goto release;
    }

    actorRenderComposeCoord(coord);
    work->age++;

    switch (task->state) {
        case 0:
            seed                 = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            m                    = &coord->coord;
            MATRIX_PAIR(m, 0, 0) = 0x1000;
            MATRIX_PAIR(m, 0, 2) = 0;
            MATRIX_PAIR(m, 1, 1) = 0x1000;
            MATRIX_PAIR(m, 2, 0) = 0;
            m->m[2][2]           = 0x1000;
            work->pos.vx         = task->spawnArg1.value & 0xFFF;
            gRandomLcgState      = seed;
            work->index          = (gRandomLcgState >> 16) % 10;
            gRandomLcgState      = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->angle          = (gRandomLcgState >> 16) % 10 + 5;
            if ((work->move.vx | work->move.vy | work->move.vz) == 0) {
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->move.vx   = 0x80 - ((gRandomLcgState >> 16) & 0xFF);
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->move.vy   = 0x80 - ((gRandomLcgState >> 16) & 0xFF);
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->move.vz   = 0x80 - ((gRandomLcgState >> 16) & 0xFF);
                if (task->spawnArg1.value < 0) {
                    work->scale = 8;
                } else if (task->spawnArg1.value & 0x40000000) {
                    work->scale = 0x80;
                } else {
                    work->scale = 0x20;
                }
                gte_SetRotMatrix(&work->parent->coord);
                gte_ldv0(&work->move);
                gte_rtv0();
                gte_stsv(&work->move);
            }
            VectorNormalSS(&work->move, &work->move);
            gte_lddp(work->scale);
            gte_ldsv(&work->move);
            gte_gpf12();
            gte_stsv(&work->move);
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            task->state         = 1;
            break;
        case 1:
            if (gRoomEffectState->effectControl == ROOM_EFFECT_CONTROL_RUNNING) {
                work->index++;
                work->move.vy--;
                coord->coord.t[0]  += work->move.vx;
                coord->coord.t[1]  += work->move.vy;
                coord->coord.t[2]  += work->move.vz;
                coord->composeStamp = GRAPHICS_COORD_DIRTY;
            } else {
                work->age--;
            }
            tick = work->age;
            end  = work->angle;
            if (tick < end - 10) {
                func_dryfield_night_motel_balcony_8018221C(task, NULL, tick);
            } else if (tick < end) {
                color[0] = color[1] = color[2] = (end - tick) * 12;
                func_dryfield_night_motel_balcony_8018221C(task, color, tick);
            } else {
            release:
                effectKillTask(work, task);
            }
            break;
    }
}

/// Draws a drifting effect task's sprite: projects its model's world position
/// through `GsWSMATRIX` and, when the GTE flag is non-negative, queues one
/// semi-transparent `POLY_FT4` (tpage 0x2B). The animation frame is
/// `index % 10`; it picks the CLUT column and one 48-texel cell of a 5x2
/// grid starting at v=0x28. The quad is centred on the projected point with a
/// half-width of `pos.vx * 47 / depth` and extends three quarters above and one
/// quarter below. `color` is the RGB the texture is modulated by; NULL draws
/// the texture raw.
/// `tick` is unused.
static void func_dryfield_night_motel_balcony_8018221C(Task* task, u8* color, s16 tick)
{
    EffectWork*          work = task->spawnArg2.pointer;
    GfxCoord*            coord;
    EffectCentreScratch* block;
    POLY_FT4*            prim;
    DisplayState*        ds;
    s16                  frame;
    s32                  u0;
    s32                  u1;
    s32                  vTop;
    s32                  vBottom;
    s16                  xy;

    frame = work->index % 10;
    coord = task->extra.coordBody->coord;

    SCRATCH_STACK_RESERVE_BLOCK(EffectCentreScratch);
    block                = SCRATCH_STACK_CURSOR(EffectCentreScratch);
    block->worldPoint.vx = coord->workm.t[0];
    block->worldPoint.vy = coord->workm.t[1];
    block->worldPoint.vz = coord->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->worldPoint);
    gte_rtps();
    gte_stsxy(&block->screenX);
    gte_stflg(&block->projectionFlags);
    if (block->projectionFlags >= 0) {
        gte_stszotz(&block->depth);
        prim           = gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2C);
        if (color != NULL) {
            prim->r0 = color[0];
            prim->g0 = color[1];
            prim->b0 = color[2];
        } else {
            setcode(prim, 0x2D);
        }
        prim->tpage = 0x2B;
        prim->clut  = getClut(frame * 16 + 0x40, 0x10E);
        setSemiTrans(prim, 1);
        u0                    = frame % 5 * 48;
        vTop                  = frame / 5 * 48;
        u1                    = u0 + 0x2F;
        vBottom               = vTop + 0x57;
        vTop                  = vTop + 0x28;
        prim->u0              = u0;
        prim->v0              = vTop;
        prim->u1              = u1;
        prim->v1              = vTop;
        prim->u2              = u0;
        prim->v2              = vBottom;
        prim->u3              = u1;
        prim->v3              = vBottom;
        block->screenExtent   = work->pos.vx * 47 / block->depth;
        xy                    = block->screenX - block->screenExtent;
        prim->x2              = xy;
        prim->x0              = xy;
        xy                    = block->screenX + block->screenExtent;
        prim->x3              = xy;
        prim->x1              = xy;
        block->screenExtent >>= 1;
        xy                    = block->screenY - block->screenExtent * 3;
        prim->y1              = xy;
        prim->y0              = xy;
        xy                    = block->screenY + block->screenExtent;
        prim->y3              = xy;
        prim->y2              = xy;
        ds                    = &gDisplayState;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->depth << ds->otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                prim);
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectCentreScratch);
}

/// Spawns an 8-step burst of effect 0x6007E and then a 6-step burst of 0x60070
/// around part 3 of the model owned by the slot-4 task's child. Each step rolls
/// the room LCG four times (three for the second burst) and builds the offset
/// vector from the top byte of each draw; the first burst also carries the last
/// draw's low nine bits, biased by 0x300, in the spawn argument.
void func_dryfield_night_motel_balcony_8018257C(void)
{
    Task*     task;
    GfxCoord* coord;
    SVECTOR   sv;
    s32       i;

    task  = gameGetTaskSlot(GAME_TASK_SLOT_SCENE);
    coord = task->firstChild->extra.tmd->coords + 3;

    for (i = 0; i < 8; i++) {
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        sv.vx           = 0x80 - ((gRandomLcgState >> 16) & 0xFF);
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        sv.vy           = 0xFE80 - ((gRandomLcgState >> 16) & 0xFF);
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        sv.vz           = 0x680 - ((gRandomLcgState >> 16) & 0xFF);
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        Gp_SpawnEff(EFFECT_NIGHT_MOTEL_BALCONY_FLAME, coord, ((gRandomLcgState >> 16) & 0x1FF) + 0x300, &sv);
    }

    for (i = 0; i < 6; i++) {
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        sv.vx           = 0x80 - ((gRandomLcgState >> 16) & 0xFF);
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        sv.vy           = 0xFE80 - ((gRandomLcgState >> 16) & 0xFF);
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        sv.vz           = 0x680 - ((gRandomLcgState >> 16) & 0xFF);
        Gp_SpawnEff(EFFECT_SMOKE_PUFF, coord, 0xC0033800, &sv);
    }
}

/// Draws once from the shared LCG (`gRandomLcgState`) and, on a draw whose upper half is
/// a multiple of three, rolls it again and spawns effect 0x6007E at part 3 of
/// the model owned by the slot-4 task's child, carrying the second draw's low
/// nine bits in the upper half of the spawn argument.
void func_dryfield_night_motel_balcony_80182730(void)
{
    Task* task;

    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    if ((u16)((gRandomLcgState >> 16) % 3U) == 0) {
        task            = gameGetTaskSlot(GAME_TASK_SLOT_SCENE);
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        Gp_SpawnEff(EFFECT_NIGHT_MOTEL_BALCONY_FLAME, task->firstChild->extra.tmd->coords + 3,
                    ((gRandomLcgState >> 16) & 0x1FF) + 0x80000100,
                    &D_dryfield_night_motel_balcony_80182D20);
    }
}
