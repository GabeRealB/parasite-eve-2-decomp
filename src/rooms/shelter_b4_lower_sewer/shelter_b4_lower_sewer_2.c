#include "rooms/shelter_b4_lower_sewer.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "common.h"
#include "gte.h"

#include "shelter_b4_lower_sewer_private.h"

#include "actors/task_tables.h"

#include "gameplay/display.h"
#include "gameplay/actor_render.h"
#include "gameplay/area.h"
#include "gameplay/areaplace.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/direction_input.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/effects.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/light.h"
#include "gameplay/loading.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gameflag.h"
#include "main/gamemain.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "overlay.h"

#include "rooms/room_common.h"

extern SVECTOR D_shelter_b4_lower_sewer_80181EA4[];
extern SVECTOR D_shelter_b4_lower_sewer_80181F04[];
extern SVECTOR D_shelter_b4_lower_sewer_80181F14[];

/// The two points the ribbon trail is emitted from, relative to the effect's
/// parent: the first positions the effect's own coordinate, the second is the
/// other end of the trail.
/// The second of those points, which the trail's per-frame state reaches
/// through its own label rather than by indexing the pair.

static void func_shelter_b4_lower_sewer_8017E6A0(SVECTOR* arg0, s32 arg1, s32 arg2);
static void func_shelter_b4_lower_sewer_8017F038(GfxCoord* arg0, s32 arg1, s32 arg2);
static void func_shelter_b4_lower_sewer_8017F828(GfxCoord* arg0, s32 arg1, s32 arg2, s32 arg3);
static void func_shelter_b4_lower_sewer_8017FC14(GfxCoord* arg0, s32 arg1, s32 arg2);
static void func_shelter_b4_lower_sewer_80180154(GfxCoord* arg0, s32 arg1, s32 arg2, u8* rgb);
static void func_shelter_b4_lower_sewer_80180580(GfxCoord* arg0, s16 arg1, u8* rgb);
static void func_shelter_b4_lower_sewer_80180E04(GfxCoord* arg0, GfxCoord* arg1, s16 arg2, s16 arg3);
static void func_shelter_b4_lower_sewer_80181484(GfxCoord* arg0, s16 arg1, u8* arg2);

extern TaskDesc D_80142604;
extern TaskDesc D_80147E48;

SVECTOR D_shelter_b4_lower_sewer_80181EA4[12] = {
    { -0x3B42, -5090, 2290, 0 },
    { -0x3692, -5090, 2290, 0 },
    { -0x2B8E, -5090, 2290, 0 },
    { -9950, -5090, 2290, 0 },
    { -7780, -5090, 2290, 0 },
    { -6580, -5090, 2290, 0 },
    { -4250, -5090, 2290, 0 },
    { -3050, -5090, 2290, 0 },
    { -780, -5090, 2290, 0 },
    { -420, -5090, 2290, 0 },
    { 2730, -5090, 2290, 0 },
    { 3930, -5090, 2290, 0 },
};

SVECTOR D_shelter_b4_lower_sewer_80181F04[2] = {
    { 6730, -5090, 2290, 0 },
    { 7930, -5090, 2290, 0 },
};

SVECTOR D_shelter_b4_lower_sewer_80181F14[16] = {
    { 0x29EA, -5090, 2290, 0 },
    { 0x2E9A, -5090, 2290, 0 },
    { 0x2E9A, -5090, -1290, 0 },
    { 0x29EA, -5090, -1290, 0 },
    { 7930, -5090, -1290, 0 },
    { 6730, -5090, -1290, 0 },
    { 3930, -5090, -1290, 0 },
    { 2730, -5090, -1290, 0 },
    { 420, -5090, -1290, 0 },
    { -780, -5090, -1290, 0 },
    { -3050, -5090, -1290, 0 },
    { -4250, -5090, -1290, 0 },
    { -6580, -5090, -1290, 0 },
    { -7780, -5090, -1290, 0 },
    { -0x3282, -1800, 370, 0 },
    { -0x3282, -1800, 1130, 0 },
};

SVECTOR D_shelter_b4_lower_sewer_80181F94[2] = {
    { 0, 190, -15, 0 },
    { 0, 1085, 180, 0 },
};

u8* D_shelter_b4_lower_sewer_80181FA4[1] = {
    D_8010CAF8,
};

GpViewCountRec D_shelter_b4_lower_sewer_80181FA8[1] = {
    { { .bytes = { 9, 0 } } },
};

GpWarpRec D_shelter_b4_lower_sewer_80181FAC[4] = {
    { { .words = { 1024, -7821, 0, 2453 } }, { 0, 0, 0, 0 }, { .words = { 1024, -7800, 0, 1620 } }, { 0, 0, 0, 0 }, 0, 0, 0, 5, 0, 447 },
    { { .words = { 3072, 0x3075, 0, 1820 } }, { 0, 0, 0, 0 }, { .words = { 3072, 0x3075, 0, 1180 } }, { 0, 0, 0, 0 }, 0x542B0002, 0x542B0001, 0, 2, 0, 0 },
    { { .words = { 1024, -6000, -2000, -1180 } }, { 0, 0, 0, 0 }, { .words = { 768, -5400, -2000, -400 } }, { 0, 0, 0, 0 }, 0, 0, 0, 9, 0, 0 },
    { { .words = { 2048, 0x37DC, -2000, -410 } }, { 0, 0, 0, 0 }, { .words = { 3328, 0x3908, -2000, -1400 } }, { 0, 0, 0, 0 }, 0x542B0006, 0x542B0005, 0, 6, 0, 0 },
};

// Retained data: Eight-point coordinate pool explicitly addressed by the following pointer table; no runtime owner found.
SVECTOR D_shelter_b4_lower_sewer_8018208C[8] = {
    { 8256, 1000, 1000, 0 },
    { 3584, 1000, 1000, 0 },
    { -2592, 1000, 1000, 0 },
    { -6528, 1000, 1000, 0 },
    { -1700, 1000, 1000, 0 },
    { 2000, 1000, 1000, 0 },
    { 7300, 1000, 1000, 0 },
    { 0x2CEC, 1000, 1000, 0 },
};

// Retained data: Eight-point coordinate pool explicitly addressed by the following pointer table; no runtime owner found.
SVECTOR D_shelter_b4_lower_sewer_801820CC[8] = {
    { 8256, 1000, 2000, 0 },
    { 3584, 1000, 2000, 0 },
    { -2592, 1000, 2000, 0 },
    { -6528, 1000, 2000, 0 },
    { -1700, 1000, 2000, 0 },
    { 2000, 1000, 2000, 0 },
    { 7300, 1000, 2000, 0 },
    { 0x2CEC, 1000, 2000, 0 },
};

// Retained data: Four retained references to the preceding eight-point coordinate pools; no runtime owner found.
SVECTOR* D_shelter_b4_lower_sewer_8018210C[4] = {
    D_shelter_b4_lower_sewer_8018208C,
    D_shelter_b4_lower_sewer_801820CC,
    D_shelter_b4_lower_sewer_8018208C,
    D_shelter_b4_lower_sewer_801820CC,
};

// Retained data: Adjacent retained point data, ending with fourth halfword -1; no runtime owner found.
SVECTOR D_shelter_b4_lower_sewer_8018211C[10] = {
    { 0, 0, 0, 0 },
    { -4900, 1000, 1000, 0 },
    { -2600, 1000, 1000, 0 },
    { 200, 1000, 1000, 0 },
    { 2000, 1000, 1000, 0 },
    { 3800, 1000, 1000, 0 },
    { 5400, 1000, 1000, 0 },
    { 8400, 1000, 1000, 0 },
    { 0x2CEC, 1000, 1000, 0 },
    { 0, 0, 0, -1 },
};

SVECTOR D_shelter_b4_lower_sewer_8018216C[16] = {
#include "assets/shelter_b4_lower_sewer_collision_05324_normals.inc"
};

SVECTOR D_shelter_b4_lower_sewer_801821EC[100] = {
#include "assets/shelter_b4_lower_sewer_collision_05324_verts.inc"
};

GpGridFace D_shelter_b4_lower_sewer_8018250C[33] = {
#include "assets/shelter_b4_lower_sewer_collision_05324_faces.inc"
};

s16 D_shelter_b4_lower_sewer_80182698[262] = {
#include "assets/shelter_b4_lower_sewer_collision_05324_cells.inc"
};

#define GRID_CELL(i) (&D_shelter_b4_lower_sewer_80182698[i])
s16* D_shelter_b4_lower_sewer_801828A4[16] = {
#include "assets/shelter_b4_lower_sewer_collision_05324_table.inc"
};
#undef GRID_CELL

GpGridParams D_shelter_b4_lower_sewer_801828E4 = { NULL, D_shelter_b4_lower_sewer_8018216C, D_shelter_b4_lower_sewer_801821EC, D_shelter_b4_lower_sewer_8018250C, D_shelter_b4_lower_sewer_801828A4, 0x39A8, 3210, 8, 2, 4000, 33 };

GpViewRec D_shelter_b4_lower_sewer_80182908[9] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { 0, 0x7530, 0 } }, 207 },
    { { { { -855, 0, -4005 }, { -579, 4052, 123 }, { 3963, 592, -846 } }, { -4200, 2980, -1880 } }, 257 },
    { { { { -601, 0, -4051 }, { -787, 4017, 116 }, { 3974, 795, -589 } }, { 3545, 3080, -2150 } }, 257 },
    { { { { -804, 0, 4016 }, { 783, 4017, 156 }, { -3939, 799, -788 } }, { -5335, 3080, -2150 } }, 257 },
    { { { { -384, 0, 4077 }, { 623, 4047, 58 }, { -4029, 626, -379 } }, { 2470, 2980, -1245 } }, 257 },
    { { { { 842, 0, -4008 }, { -693, 4034, -145 }, { 3947, 709, 829 } }, { -7260, 4330, 1250 } }, 257 },
    { { { { -1021, 0, -3966 }, { 547, 4056, -140 }, { 3928, -565, -1011 } }, { 0, 2490, -130 } }, 257 },
    { { { { -780, 0, 4020 }, { -569, 4054, -110 }, { -3980, -580, -772 } }, { -4910, 2670, -90 } }, 257 },
    { { { { -974, 0, 3978 }, { 865, 3997, 211 }, { -3883, 891, -950 } }, { 410, 4410, -280 } }, 257 },
};

SpriteBatch D_shelter_b4_lower_sewer_80182A4C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b4_lower_sewer_80182A5C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b4_lower_sewer_80182A6C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_shelter_b4_lower_sewer_80182A7C[3] = {
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 40, -72, 3800, { .fields = { 96, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 32 } }, 32, -48, 3775, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 32 } }, 32, -16, 3750, { .fields = { 88, 32 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b4_lower_sewer_80182AB8[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 3, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_shelter_b4_lower_sewer_80182AD0[16] = {
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 24, -72, 1800, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 32 } }, 40, -48, 1925, { .fields = { 88, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 24 } }, 40, 64, 1625, { .fields = { 64, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 24, 64, 1420, { .fields = { 112, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 16 } }, 40, 48, 1675, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 24, 48, 1438, { .fields = { 112, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 16 } }, 40, 32, 1725, { .fields = { 56, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 24, 32, 1488, { .fields = { 96, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 16 } }, 40, 16, 1775, { .fields = { 56, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 24, 16, 1506, { .fields = { 112, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 16 } }, 40, 0, 1825, { .fields = { 80, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 24, 0, 1700, { .fields = { 112, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 16 } }, 40, -16, 1875, { .fields = { 80, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 24, -16, 1750, { .fields = { 112, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 80, 88, 1377, { .fields = { 104, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 24, 88, 1450, { .fields = { 104, 248 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b4_lower_sewer_80182C10[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 16, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_shelter_b4_lower_sewer_80182C28[9] = {
    { 143, 0x3FC0, { .fields = { 64, 8 } }, -8, 48, 1725, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 8 } }, -8, 56, 1618, { .fields = { 64, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 72, 8 } }, -16, 64, 1575, { .fields = { 56, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 80, 8 } }, -24, 72, 1452, { .fields = { 48, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 80, 8 } }, -24, 80, 1425, { .fields = { 48, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 88, 8 } }, -32, 88, 1350, { .fields = { 40, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 88, 8 } }, -32, 96, 1275, { .fields = { 40, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 96, 8 } }, -40, 104, 1200, { .fields = { 32, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 96, 8 } }, -40, 112, 1125, { .fields = { 32, 64 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b4_lower_sewer_80182CDC[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 9, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b4_lower_sewer_80182CF4[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b4_lower_sewer_80182D04[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

GpSprtElem D_shelter_b4_lower_sewer_80182D14[17] = {
    { 143, 0x3FC0, { .fields = { 64, 8 } }, -8, 16, 1690, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 8 } }, -8, 24, 1686, { .fields = { 64, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 72, 8 } }, -16, 32, 1626, { .fields = { 56, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 72, 8 } }, -16, 40, 1625, { .fields = { 56, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 80, 8 } }, -32, 48, 1500, { .fields = { 48, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 80, 8 } }, -32, 56, 1425, { .fields = { 48, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 80, 8 } }, -32, 64, 1400, { .fields = { 48, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 80, 8 } }, -32, 72, 1250, { .fields = { 48, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 80, 8 } }, -32, 80, 1175, { .fields = { 48, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 80, 8 } }, -32, 88, 1125, { .fields = { 48, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 80, 8 } }, -40, 96, 1075, { .fields = { 48, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 80, 8 } }, -40, 104, 1025, { .fields = { 48, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 80, 8 } }, -40, 112, 975, { .fields = { 48, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 8 } }, -8, 8, 1742, { .fields = { 64, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 8 } }, -8, 0, 1808, { .fields = { 64, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 8 } }, -8, -8, 1873, { .fields = { 64, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 8 } }, -8, -16, 2005, { .fields = { 64, 128 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b4_lower_sewer_80182E68[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 17, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

GpSprtRec D_shelter_b4_lower_sewer_80182E80[9] = {
    { { .empty = D_shelter_b4_lower_sewer_80182A4C }, D_shelter_b4_lower_sewer_80182A4C, NULL },
    { { .empty = D_shelter_b4_lower_sewer_80182A5C }, D_shelter_b4_lower_sewer_80182A5C, NULL },
    { { .empty = D_shelter_b4_lower_sewer_80182A6C }, D_shelter_b4_lower_sewer_80182A6C, NULL },
    { { .elements = D_shelter_b4_lower_sewer_80182A7C }, D_shelter_b4_lower_sewer_80182AB8, NULL },
    { { .elements = D_shelter_b4_lower_sewer_80182AD0 }, D_shelter_b4_lower_sewer_80182C10, NULL },
    { { .elements = D_shelter_b4_lower_sewer_80182C28 }, D_shelter_b4_lower_sewer_80182CDC, NULL },
    { { .empty = D_shelter_b4_lower_sewer_80182CF4 }, D_shelter_b4_lower_sewer_80182CF4, NULL },
    { { .empty = D_shelter_b4_lower_sewer_80182D04 }, D_shelter_b4_lower_sewer_80182D04, NULL },
    { { .elements = D_shelter_b4_lower_sewer_80182D14 }, D_shelter_b4_lower_sewer_80182E68, NULL },
};

GpPointLight D_shelter_b4_lower_sewer_80182EEC[14] = {
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -7100, -1900, 1500 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2457, 2457, 2457, { 0, 0 } }, 3662, 5242 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -0x3908, -4600, 2199 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2457, 2457, 2457, { 0, 0 } }, 2500, 3500 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -3700, -4000, -430 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 1638, 1638, 1638, { 0, 0 } }, 1500, 2500 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -99, -4000, -430 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 1638, 1638, 1638, { 0, 0 } }, 4341, 4920 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 3400, -4000, -430 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 1638, 1638, 1638, { 0, 0 } }, 1500, 2500 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 7400, -4000, -430 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 1638, 1638, 1638, { 0, 0 } }, 4162, 5341 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x2C88, -4000, -430 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 1638, 1638, 1638, { 0, 0 } }, 4501, 4898 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -7100, -4000, -430 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 1638, 1638, 1638, { 0, 0 } }, 1500, 2500 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -0x2EE5, -1800, 749 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2457, 2457, 2457, { 0, 0 } }, 3859, 3860 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0x2C88, -1900, 1500 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2457, 2457, 2457, { 0, 0 } }, 2000, 3000 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 7400, -1900, 1500 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2457, 2457, 2457, { 0, 0 } }, 3121, 4161 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 3400, -1900, 1500 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2457, 2457, 2457, { 0, 0 } }, 2481, 4983 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -99, -1900, 1500 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2457, 2457, 2457, { 0, 0 } }, 2300, 3921 },
    { { { .coord = { 0, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -3700, -1900, 1500 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, { .rot = { 0, 0, 0, 0 } }, NULL } }, 2457, 2457, 2457, { 0, 0 } }, 2300, 5121 },
};

GpRoomCoordSet D_shelter_b4_lower_sewer_8018342C = { 0, NULL, 14, D_shelter_b4_lower_sewer_80182EEC, 0, NULL };

GpObj4C D_shelter_b4_lower_sewer_80183444[12] = {
    { NULL, NULL, NULL, { 0x2A5F, -3585, -1922, 0 }, { { -6, -3824, 2091, 0 }, { -7, -3824, -2101, 0 }, { -6, 3824, 2091, 0 }, { -7, 3824, -2101, 0 } }, { -4102, 0, 0, 0 }, { 0, 0, 4096, 0 }, 4344, 0, 7, 6, 1, 0 },
    { NULL, NULL, NULL, { 0x299F, -3569, -2081, 0 }, { { -4, -3904, -2130, 0 }, { -6, -3904, 2121, 0 }, { -4, 3904, -2130, 0 }, { -6, 3904, 2121, 0 } }, { 4098, 0, 1, 0 }, { 0, 0, 4096, 0 }, 4434, 0, 6, 7, 1, 0 },
    { NULL, NULL, NULL, { 2495, -3553, -1985, 0 }, { { -2, -3760, 2094, 0 }, { -2, -3760, -2098, 0 }, { -2, 3761, 2094, 0 }, { -2, 3761, -2098, 0 } }, { -4096, 0, 0, 0 }, { 0, 0, 4096, 0 }, 4283, 0, 8, 7, 1, 0 },
    { NULL, NULL, NULL, { 2367, -3665, -1985, 0 }, { { 92, -3744, -2128, 0 }, { -95, -3744, 2125, 0 }, { 92, 3744, -2128, 0 }, { -95, 3744, 2125, 0 } }, { 4090, 0, 179, 0 }, { 0, 0, 4096, 0 }, 4283, 0, 7, 8, 1, 0 },
    { NULL, NULL, NULL, { -3266, -3617, -2017, 0 }, { { 0, -3696, 2095, 0 }, { -1, -3696, -2096, 0 }, { 0, 3696, 2095, 0 }, { -1, 3696, -2096, 0 } }, { -4098, 0, 0, 0 }, { 0, 0, 4096, 0 }, 4222, 0, 9, 8, 1, 0 },
    { NULL, NULL, NULL, { -3489, -3633, -1985, 0 }, { { 0, -3776, -2128, 0 }, { -1, -3776, 2127, 0 }, { 0, 3777, -2128, 0 }, { -1, 3777, 2127, 0 } }, { 4103, 0, 0, 0 }, { 0, 0, 4096, 0 }, 4314, 0, 8, 9, 1, 0 },
    { NULL, NULL, NULL, { 9023, -1633, 1951, 0 }, { { -390, -2864, -2096, 0 }, { 383, -2864, 2088, 0 }, { -390, 2864, -2096, 0 }, { 383, 2864, 2088, 0 } }, { 4028, 0, -745, 0 }, { 0, 0, 4096, 0 }, 3565, 0, 2, 3, 1, 0 },
    { NULL, NULL, NULL, { 9343, -1600, 1887, 0 }, { { 300, -2928, 2067, 0 }, { -314, -2928, -2080, 0 }, { 300, 2928, 2067, 0 }, { -314, 2928, -2080, 0 } }, { -4052, 0, 599, 0 }, { 0, 0, 4096, 0 }, 3593, 0, 3, 2, 1, 0 },
    { NULL, NULL, NULL, { 1151, -1617, 2111, 0 }, { { -1, -2880, 2095, 0 }, { -1, -2880, -2097, 0 }, { -1, 2880, 2095, 0 }, { -1, 2880, -2097, 0 } }, { -4105, 0, 0, 0 }, { 0, 0, 4096, 0 }, 3556, 0, 4, 3, 1, 0 },
    { NULL, NULL, NULL, { 895, -1617, 2080, 0 }, { { -1, -2880, -2129, 0 }, { -2, -2880, 2126, 0 }, { -1, 2880, -2129, 0 }, { -2, 2880, 2126, 0 } }, { 4103, 0, 0, 0 }, { 0, 0, 4096, 0 }, 3574, 0, 3, 4, 1, 0 },
    { NULL, NULL, NULL, { -6914, -1649, 1951, 0 }, { { -210, -2880, -2117, 0 }, { 209, -2880, 2116, 0 }, { -210, 2880, -2117, 0 }, { 209, 2880, 2116, 0 } }, { 4081, 0, -405, 0 }, { 0, 0, 4096, 0 }, 3574, 0, 4, 5, 1, 0 },
    { NULL, NULL, NULL, { -6818, -1600, 1887, 0 }, { { 101, -2896, 2092, 0 }, { -103, -2896, -2094, 0 }, { 101, 2896, 2092, 0 }, { -103, 2896, -2094, 0 } }, { -4091, 0, 198, 0 }, { 0, 0, 4096, 0 }, 3574, 0, 5, 4, 129, 0 },
};

GpObj4C D_shelter_b4_lower_sewer_801837D4[12] = {
    { NULL, NULL, NULL, { -8288, -48, 2976, 0 }, { { -544, 0, -1024, 0 }, { 544, 0, -1024, 0 }, { -544, 0, 1024, 0 }, { 544, 0, 1024, 0 } }, { 0, 4102, 0, 0 }, { 4096, 0, 0, 0 }, 1159, 1, 53, 192, 2, 0 },
    { NULL, NULL, NULL, { -8736, -1888, 2384, 0 }, { { 0, 1600, -784, 0 }, { 0, -1600, -784, 0 }, { 0, 1600, 784, 0 }, { 0, -1600, 784, 0 } }, { 4109, 0, 0, 0 }, { 4096, 0, 0, 0 }, 1778, 0x8000, 41, 18, 2, 0 },
    { NULL, NULL, NULL, { 0x3200, -48, 1472, 0 }, { { -544, 0, -1024, 0 }, { 544, 0, -1024, 0 }, { -544, 0, 1024, 0 }, { 544, 0, 1024, 0 } }, { 0, 4102, 0, 0 }, { -4096, 0, 0, 0 }, 1159, 0, 44, 33, 2, 0 },
    { NULL, NULL, NULL, { 0x3760, -2080, -224, 0 }, { { 1024, 0, -544, 0 }, { 1024, 0, 544, 0 }, { -1024, 0, -544, 0 }, { -1024, 0, 544, 0 } }, { 0, 4102, 0, 0 }, { 0, 0, -4096, 0 }, 1159, 0, 44, 68, 2, 0 },
    { NULL, NULL, NULL, { -6528, -2048, -1024, 0 }, { { -544, 0, -1024, 0 }, { 544, 0, -1024, 0 }, { -544, 0, 1024, 0 }, { 544, 0, 1024, 0 } }, { 0, 4102, 0, 0 }, { 4096, 0, 0, 0 }, 1159, 1, 85, 192, 2, 0 },
    { NULL, NULL, NULL, { -7072, -2737, -992, 0 }, { { 0, 1296, -1024, 0 }, { 0, -1295, -1024, 0 }, { 0, 1296, 1024, 0 }, { 0, -1295, 1024, 0 } }, { 4102, 0, 0, 0 }, { 4096, 0, 0, 0 }, 1649, 0x8000, 42, 50, 2, 0 },
    { NULL, NULL, NULL, { -0x30E0, -64, 736, 0 }, { { -544, 0, -1024, 0 }, { 544, 0, -1024, 0 }, { -544, 0, 1024, 0 }, { 544, 0, 1024, 0 } }, { 0, 4102, 0, 0 }, { 4096, 0, 0, 0 }, 1159, 2, 1, 0, 2, 0 },
    { NULL, NULL, NULL, { -176, -64, 304, 0 }, { { -1072, 0, -720, 0 }, { 1072, 0, -720, 0 }, { -1072, 0, 720, 0 }, { 1072, 0, 720, 0 } }, { 0, 4101, 0, 0 }, { 0, 0, 4096, 0 }, 1286, 2, 3, 0, 2, 0 },
    { NULL, NULL, NULL, { 704, -64, 2496, 0 }, { { -1360, 0, -720, 0 }, { 1360, 0, -720, 0 }, { -1360, 0, 720, 0 }, { 1360, 0, 720, 0 } }, { 0, 4096, 0, 0 }, { 0, 0, -4096, 0 }, 1536, 2, 2, 0, 2, 0 },
    { NULL, NULL, NULL, { 2528, -2048, -2304, 0 }, { { -1360, 0, -720, 0 }, { 1360, 0, -720, 0 }, { -1360, 0, 720, 0 }, { 1360, 0, 720, 0 } }, { 0, 4096, 0, 0 }, { 0, 0, 4096, 0 }, 1536, 2, 4, 0, 2, 0 },
    { NULL, NULL, NULL, { 6976, -64, 2528, 0 }, { { -1360, 0, -720, 0 }, { 1360, 0, -720, 0 }, { -1360, 0, 720, 0 }, { 1360, 0, 720, 0 } }, { 0, 4096, 0, 0 }, { 0, 0, -4096, 0 }, 1536, 2, 2, 0, 2, 0 },
    { NULL, NULL, NULL, { -2784, -64, 2560, 0 }, { { -1360, 0, -720, 0 }, { 1360, 0, -720, 0 }, { -1360, 0, 720, 0 }, { 1360, 0, 720, 0 } }, { 0, 4096, 0, 0 }, { 0, 0, -4096, 0 }, 1536, 2, 2, 0, 130, 0 },
};

GpAreaTmdRec D_shelter_b4_lower_sewer_80183B64[2] = {
    { 44, 44, 0, 0, { 0, 0 }, &D_80142604 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_shelter_b4_lower_sewer_80183B7C[2] = {
    { 4, 4, 0, 0, { 0, 0 }, &D_80147E48 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_shelter_b4_lower_sewer_80183B94[4] = {
    { 4, 4, 0, 0, { 0, 0 }, &D_80147E48 },
    { 72, 72, 1, 0, { 0, 0 }, D_80153EC8 },
    { 73, 73, 1, 0, { 0, 0 }, D_8014E7A4 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_shelter_b4_lower_sewer_80183BC4[2] = {
    { 20, 20, 0, 0, { 0, 0 }, D_80147DF0 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_shelter_b4_lower_sewer_80183BDC[2] = {
    { 23, 23, 0, 0, { 0, 0 }, D_80147AB8 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_shelter_b4_lower_sewer_80183BF4[3] = {
    { 4, 4, 0, 0, { 0, 0 }, &D_80147E48 },
    { 49, 49, 1, 0, { 0, 0 }, D_8015F400 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

AreaPlacement D_shelter_b4_lower_sewer_80183C18[3] = {
    { 44, 0, 0, -500, 0, 700, 2700, 0, 0, 2, 0 },
    { 44, 0, 0, 4500, 0, 2500, 3400, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaPlacement D_shelter_b4_lower_sewer_80183C48[2] = {
    { 4, 0, 1, 0x2CEC, 1000, 1000, 1280, 0, 0, 2, 4 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaPlacement D_shelter_b4_lower_sewer_80183C68[6] = {
    { 4, 0, 1, 0x2CEC, 1000, 1000, 3072, 0, 0, 2, 2 },
    { 4, 0, 17, -5000, 1000, 2000, 1024, 0, 0, 2, 2 },
    { 72, 0, 0, 0, -2000, -930, 1024, 0, 2, 4, 0 },
    { 73, 0, 1, -3600, -2000, -1400, 1200, 0, 2, 4, 0 },
    { 73, 0, 1, 2250, -2000, -400, 700, 0, 2, 4, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaPlacement D_shelter_b4_lower_sewer_80183CC8[2] = {
    { 20, 4, 1, -2000, 0, 1600, 1024, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaPlacement D_shelter_b4_lower_sewer_80183CE8[2] = {
    { 23, 5, 1, 7000, -2000, -1000, 3072, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaPlacement D_shelter_b4_lower_sewer_80183D08[4] = {
    { 4, 0, 1, 0x2CEC, 1000, 1000, 3072, 0, 0, 2, 3 },
    { 4, 0, 17, -5000, 1000, 2000, 1024, 0, 0, 2, 3 },
    { 49, 1, 0, 1950, -2000, -1300, 200, 0, 2, 4, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

GpAreaVariant D_shelter_b4_lower_sewer_80183D48[12] = {
    { NULL, NULL },
    { D_shelter_b4_lower_sewer_80183C18, D_shelter_b4_lower_sewer_80183B64 },
    { D_shelter_b4_lower_sewer_80183C48, D_shelter_b4_lower_sewer_80183B7C },
    { D_shelter_b4_lower_sewer_80183C68, D_shelter_b4_lower_sewer_80183B94 },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { D_shelter_b4_lower_sewer_80183CC8, D_shelter_b4_lower_sewer_80183BC4 },
    { D_shelter_b4_lower_sewer_80183CE8, D_shelter_b4_lower_sewer_80183BDC },
    { NULL, NULL },
    { NULL, NULL },
    { D_shelter_b4_lower_sewer_80183D08, D_shelter_b4_lower_sewer_80183BF4 },
};

s32 D_shelter_b4_lower_sewer_80183DA8[3] = {
    0x10000041,
    0x10000043,
    0x10000055,
};

s32 D_shelter_b4_lower_sewer_80183DB4[3] = {
    0x1000003D,
    0x1000003F,
    0x1000003D,
};

s32 D_shelter_b4_lower_sewer_80183DC0[3] = {
    0x10000015,
    0x10000017,
    0x10000019,
};

GpRoomParamRec D_shelter_b4_lower_sewer_80183DCC[1] = {
    { 0, 0, 1, 0, NULL },
};

GpRoomParamRec D_shelter_b4_lower_sewer_80183DD4[1] = {
    { 0, 1, 0, 0, NULL },
};

GpRoomParamRec D_shelter_b4_lower_sewer_80183DDC[1] = {
    { 0, 0, 1, 0, D_shelter_b4_lower_sewer_80183DB4 },
};

GpRoomParamRec D_shelter_b4_lower_sewer_80183DE4[1] = {
    { 0, 0, 1, 0, D_shelter_b4_lower_sewer_80183DC0 },
};

GpRoomParamRec D_shelter_b4_lower_sewer_80183DEC[1] = {
    { 0, 0, 1, 0, D_shelter_b4_lower_sewer_80183DA8 },
};

GpRoomParamRec* D_shelter_b4_lower_sewer_80183DF4[8] = {
    D_shelter_b4_lower_sewer_80183DCC,
    D_shelter_b4_lower_sewer_80183DD4,
    D_shelter_b4_lower_sewer_80183DDC,
    D_shelter_b4_lower_sewer_80183DE4,
    D_shelter_b4_lower_sewer_80183DCC,
    D_shelter_b4_lower_sewer_80183DEC,
    D_shelter_b4_lower_sewer_80183DCC,
    D_shelter_b4_lower_sewer_80183DCC,
};

u8* D_shelter_b4_lower_sewer_80183E14 = NULL;

/// Per-frame task drawing the room's glowing capsules. On its first tick it
/// stores the values 0x600ED, 0x600EE and 0x600EF in three gameplay globals,
/// and 0x6016E and 0x6016F in two more when GameFlag nibble 0xB7 is 1. Each
/// tick it then draws, through `func_shelter_b4_lower_sewer_8017E6A0`, the
/// capsules visible from the current camera view, picked from the point-pair
/// lists `D_shelter_b4_lower_sewer_80181EA4`, `D_shelter_b4_lower_sewer_80181F04`
/// and `D_shelter_b4_lower_sewer_80181F14`.
void func_shelter_b4_lower_sewer_8017E400(Task* arg0)
{
    if (arg0->state == 0) {
        D_80115758 = 0x600ED;
        D_8011572C = 0x600EE;
        D_80115750 = 0x600EF;
        if (GameFlag_GetNibble(0xB7) == 1) {
            D_8011574C = 0x6016E;
            D_80115738 = 0x6016F;
        }
        arg0->state = 1;
    }

    switch (Gp_GetViewIndex() & 0xFF) {
        case 2:
        case 6: {
            SVECTOR* p = D_shelter_b4_lower_sewer_80181F14;
            func_shelter_b4_lower_sewer_8017E6A0(&p[0], 0x200, 0x222);
            func_shelter_b4_lower_sewer_8017E6A0(&p[2], 0x200, 0x222);
            break;
        }
        case 3: {
            SVECTOR* p = D_shelter_b4_lower_sewer_80181F04;
            func_shelter_b4_lower_sewer_8017E6A0(&p[0], 0x200, 0x222);
            func_shelter_b4_lower_sewer_8017E6A0(&p[2], 0x200, 0x222);
            func_shelter_b4_lower_sewer_8017E6A0(&p[4], 0x200, 0x222);
            func_shelter_b4_lower_sewer_8017E6A0(&p[6], 0x200, 0x222);
            break;
        }
        case 4: {
            SVECTOR* p = D_shelter_b4_lower_sewer_80181EA4;
            func_shelter_b4_lower_sewer_8017E6A0(&p[0], 0x200, 0x222);
            func_shelter_b4_lower_sewer_8017E6A0(&p[2], 0x200, 0x222);
            func_shelter_b4_lower_sewer_8017E6A0(&p[4], 0x200, 0x222);
            func_shelter_b4_lower_sewer_8017E6A0(&p[6], 0x200, 0x222);
            func_shelter_b4_lower_sewer_8017E6A0(&p[24], 0x200, 0x222);
            func_shelter_b4_lower_sewer_8017E6A0(&p[26], 0x200, 0x222);
            func_shelter_b4_lower_sewer_8017E6A0(&p[28], 0x200, 0x222);
            break;
        }
        case 5: {
            SVECTOR* p = D_shelter_b4_lower_sewer_80181EA4;
            func_shelter_b4_lower_sewer_8017E6A0(&p[0], 0x200, 0x222);
            func_shelter_b4_lower_sewer_8017E6A0(&p[2], 0x200, 0x222);
            func_shelter_b4_lower_sewer_8017E6A0(&p[28], 0x200, 0x222);
            break;
        }
        case 7: {
            SVECTOR* p = D_shelter_b4_lower_sewer_80181F04;
            func_shelter_b4_lower_sewer_8017E6A0(&p[0], 0x200, 0x222);
            func_shelter_b4_lower_sewer_8017E6A0(&p[2], 0x200, 0x222);
            func_shelter_b4_lower_sewer_8017E6A0(&p[4], 0x200, 0x222);
            func_shelter_b4_lower_sewer_8017E6A0(&p[6], 0x200, 0x222);
            func_shelter_b4_lower_sewer_8017E6A0(&p[8], 0x200, 0x222);
            break;
        }
        case 8: {
            SVECTOR* p = D_shelter_b4_lower_sewer_80181EA4;
            func_shelter_b4_lower_sewer_8017E6A0(&p[0], 0x200, 0x222);
            func_shelter_b4_lower_sewer_8017E6A0(&p[2], 0x200, 0x222);
            func_shelter_b4_lower_sewer_8017E6A0(&p[4], 0x200, 0x222);
            func_shelter_b4_lower_sewer_8017E6A0(&p[6], 0x200, 0x222);
            func_shelter_b4_lower_sewer_8017E6A0(&p[8], 0x200, 0x222);
            func_shelter_b4_lower_sewer_8017E6A0(&p[22], 0x200, 0x222);
            func_shelter_b4_lower_sewer_8017E6A0(&p[24], 0x200, 0x222);
            func_shelter_b4_lower_sewer_8017E6A0(&p[26], 0x200, 0x222);
            func_shelter_b4_lower_sewer_8017E6A0(&p[28], 0x200, 0x222);
            break;
        }
        case 9: {
            SVECTOR* p = D_shelter_b4_lower_sewer_80181EA4;
            func_shelter_b4_lower_sewer_8017E6A0(&p[0], 0x200, 0x222);
            func_shelter_b4_lower_sewer_8017E6A0(&p[2], 0x200, 0x222);
            func_shelter_b4_lower_sewer_8017E6A0(&p[4], 0x200, 0x222);
            func_shelter_b4_lower_sewer_8017E6A0(&p[24], 0x200, 0x222);
            func_shelter_b4_lower_sewer_8017E6A0(&p[26], 0x200, 0x222);
            func_shelter_b4_lower_sewer_8017E6A0(&p[28], 0x200, 0x222);
            break;
        }
    }
}

/// Draws a glowing capsule between the points `arg0[0]` and `arg0[1]`,
/// projected through `gGfxViewCoord.workm`; nothing is drawn unless both project.
/// Each end is a half-disc of screen radius `arg1 * 64 / otz` and the two are
/// joined by a band, built from gouraud quads bright at the centre line and
/// black at the rim, in two 0x400 steps around the angle between the projected
/// points. `arg2` is the colour as three 4-bit channels (0xRGB), brightened
/// slightly on odd frames.
static void func_shelter_b4_lower_sewer_8017E6A0(SVECTOR* arg0, s32 arg1, s32 arg2)
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

/// Per-frame driver of an expanding, fading flash. While the room's event
/// state is 0 it updates the task's coordinate, ticks the age counter and
/// draws the flash through `func_shelter_b4_lower_sewer_8017F038` at size
/// `angle`, seeded from the spawn argument and grown by 0x20 a frame, and
/// brightness `scale`, which starts at 0x40 and drops by 2 a frame; the
/// first frame also turns the coordinate about Y by a random angle. The work
/// block is released once the brightness falls under 2. Once the event state
/// is non-zero it only draws, releasing the block from event state 4 on.
void func_shelter_b4_lower_sewer_8017EEE4(Task* task)
{
    GpEffWork* work;
    GfxCoord*  coord;

    work  = task->spawnArg2.pointer;
    coord = task->extra.coordBody->coord;
    if (Gp_State1C->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
        func_shelter_b4_lower_sewer_8017F038(coord, work->angle, work->scale);
        if (Gp_State1C->effectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            Gp_ReleaseState1CMem(work, task);
        }
    } else {
        Gp_UpdateCoord(coord);
        work->age++;
        if (task->state == 0) {
            work->scale = 0x40;
            work->angle = task->spawnArg1.halves.low & 0xFFF;
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            Gfx_RotMatrixY(&coord->coord, ((u32)Gp_LcgState >> 16) & 0xFFF, 1);
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            task->state         = 1;
        }
        work->angle += 0x20;
        func_shelter_b4_lower_sewer_8017F038(coord, work->angle, work->scale);
        work->scale -= 2;
        if (work->scale < 2) {
            Gp_ReleaseState1CMem(work, task);
        }
    }
}

/// Draws a flat textured quad at `arg0`: the four corners of the unit quad
/// `D_80111E38`, scaled by `arg1`, are rotated by the coordinate's world
/// matrix and offset by its translation, then projected through `GsWSMATRIX`.
/// If the projection is valid, one semi-transparent `POLY_FT4` (tpage 0x2B,
/// clut 0x43D1, UV 0,0x38 to 0x37,0x6F) is queued with all three colour
/// channels set to `arg2`. The work block lives on the scratchpad stack.
static void func_shelter_b4_lower_sewer_8017F038(GfxCoord* arg0, s32 arg1, s32 arg2)
{
    void**         scratch;
    u8*            head;
    GpQuadScratch* block;
    SVECTOR*       v;
    s32            i;
    GpQuadCorner*  tbl;
    POLY_FT4*      prim;
    s32            prod;

    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    head    -= 0x38;
    *scratch = head;
    block    = (GpQuadScratch*)head;
    gte_SetTransMatrix(&GsWSMATRIX);
    for (i = 0; i < 4; i++) {
        tbl   = &D_80111E38[i];
        v     = &block->vec[i];
        prod  = tbl->x * arg1;
        v->vy = 0;
        v->vx = prod;
        v->vz = tbl->y * arg1;
        gte_SetRotMatrix(&arg0->workm);
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
        setcode(prim, 0x2C);
        prim->tpage = 0x2B;
        prim->clut  = 0x43D1;
        prim->v0    = 0x38;
        prim->v1    = 0x38;
        setRGB0(prim, arg2, arg2, arg2);
        prim->u0 = 0;
        prim->u1 = 0x37;
        prim->u2 = 0;
        prim->v2 = 0x6F;
        prim->u3 = 0x37;
        prim->v3 = 0x6F;
        setSemiTrans(prim, 1);
        prim->x0 = block->sxy0.vx;
        prim->y0 = block->sxy0.vy;
        prim->x1 = block->sxy1.vx;
        prim->y1 = block->sxy1.vy;
        prim->x2 = block->sxy2.vx;
        prim->y2 = block->sxy2.vy;
        prim->x3 = block->sxy3.vx;
        prim->y3 = block->sxy3.vy;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                prim);
    }
    SCRATCH_POP_BYTES(0x38);
}

/// Per-frame update of a sprite effect drawn with
/// `func_shelter_b4_lower_sewer_8017F828` (state 1, a spinning sprite) or
/// `func_shelter_b4_lower_sewer_8017FC14` (state 2, an upright one). State 0
/// seeds the work from `spawnArg1`: the sprite size, a random spin angle, the
/// number of ticks per animation frame and, when the spawner left `move`
/// zero, a velocity chosen by bits 24-27 of `spawnArg1`, normalised and scaled
/// by `step` through the GTE. Later ticks draw, drift the coordinate by
/// that velocity with `vy` growing by 6 each tick, and advance the animation
/// frame every `period` ticks, releasing the task after frame 7. While an
/// event is running the task only draws, and it is released once the event
/// state reaches 4.
void func_shelter_b4_lower_sewer_8017F36C(Task* task)
{
    GpEffWork* work;
    GfxCoord*  coord;
    SVECTOR*   vec;
    s32        kind;
    s32        step;
    s32        state;
    s32        level;

    work  = task->spawnArg2.pointer;
    coord = task->extra.coordBody->coord;
    if (Gp_State1C->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
        if (Gp_State1C->effectControl < ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            if (task->state < 2) {
                func_shelter_b4_lower_sewer_8017F828(coord, (u16)work->index, work->scale, work->angle);
            } else {
                func_shelter_b4_lower_sewer_8017FC14(coord, (u16)work->index, work->scale);
            }
            return;
        }
        Gp_ReleaseState1CMem(work, task);
        return;
    }
    Gp_UpdateCoord(coord);
    work->age++;
    switch (task->state) {
        case 0:
            work->scale = task->spawnArg1.halves.low & 0xFFF;
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            work->angle = ((u32)Gp_LcgState >> 16) & 0xFFF;
            if (task->spawnArg1.value & 0xF000) {
                step = (task->spawnArg1.value >> 12) & 0xF;
            } else {
                step = 1;
            }
            work->period = step;
            work->age    = 0;
            state        = 1;
            if (task->spawnArg1.value & 0xF0000000) {
                state = 2;
            }
            task->state = state;
            if (((u16)work->move.vx | (u16)work->move.vy | (u16)work->move.vz) == 0) {
                if (task->spawnArg1.value & 0xFF0000) {
                    level = (task->spawnArg1.value >> 16) & 0xFF;
                } else {
                    level = 0x40;
                }
                work->step = level;
                kind       = task->spawnArg1.signedBytes[3];
                switch (kind & 0xF) {
                    case 0:
                        work->step = 0;
                        break;
                    case 1:
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vx = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vy = 0xFFC0 - (((u32)Gp_LcgState >> 16) & 0x7F);
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vz = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        break;
                    case 2:
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vx = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vy = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vz = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        break;
                    case 3:
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vx = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vy = -(((u32)Gp_LcgState >> 16) & 0xFF);
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vz = 0x10 - (((u32)Gp_LcgState >> 16) & 0x1F);
                        break;
                    case 5:
                        work->move.vx = work->pos.vx;
                        work->move.vy = work->pos.vy;
                        work->move.vz = work->pos.vz;
                        break;
                }
                vec = &work->move;
                VectorNormalSS(vec, vec);
                gte_lddp(work->step);
                gte_ldsv(vec);
                gte_gpf12();
                gte_stsv(vec);
            } else {
                work->step = 0x40;
            }
            return;
        case 1:
            func_shelter_b4_lower_sewer_8017F828(coord, (u16)work->index, work->scale, work->angle);
            break;
        case 2:
            func_shelter_b4_lower_sewer_8017FC14(coord, (u16)work->index, work->scale);
            break;
        default:
            return;
    }
    if (work->step != 0) {
        coord->coord.t[0]  += work->move.vx;
        coord->coord.t[1]  += work->move.vy;
        coord->coord.t[2]  += work->move.vz;
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
        work->move.vy      += 6;
    }
    if ((work->age % work->period) == 0) {
        work->index++;
        if (work->index >= 8) {
            Gp_ReleaseState1CMem(work, task);
        }
    }
}

/// Draws a spinning sprite at the coordinate's world position. The position is
/// projected through `GsWSMATRIX`; if the projection is valid, one
/// semi-transparent, unshaded `POLY_FT4` (tpage 0x2B, clut 0x43D3) is queued
/// as a square rotated by angle `arg3` about the projected point, with
/// on-screen half-diagonal `(s16)arg2 * 31 / otz`. `arg1` picks the 32-texel
/// frame at u = `arg1 * 32`, v 0xE0 to 0xFF.
static void func_shelter_b4_lower_sewer_8017F828(GfxCoord* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    void**           scratch;
    u8*              head;
    GpFxQuadScratch* block;
    POLY_FT4*        prim;
    SVECTOR*         vec;
    s32              u0;
    s32              ang;
    s32              ang2;
    u16              vz;

    scratch = (void**)G_SCRATCH_HEAD;
    TOUCH_REG_USE(arg2, scratch);
    head                                      = *scratch;
    ((GpFxQuadScratch*)(head - 0x1C))->vec.vx = (u16)arg0->workm.t[0];
    block                                     = (GpFxQuadScratch*)(head - 0x1C);
    block->vec.vy                             = (u16)arg0->workm.t[1];
    vz                                        = (u16)arg0->workm.t[2];
    *scratch                                  = block;
    block->vec.vz                             = vz;
    vec                                       = &block->vec;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(vec);
    gte_rtps();
    gte_stsxy(&((GpFxQuadScratch*)(head - 0x1C))->sx);
    gte_stflg(&((GpFxQuadScratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((GpFxQuadScratch*)(head - 0x1C))->otz);
        prim           = gGpuPrimCursor;
        ang            = (s16)arg3;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2F);
        prim->tpage = 0x2B;
        prim->clut  = 0x43D3;
        u0          = (arg1 & 0xFFFF) << 5;
        setUV4(prim, u0, 0xE0, u0 + 0x1F, 0xE0, u0, 0xFF, u0 + 0x1F, 0xFF);
        block->dx = ((((s16)arg2 * 31) / block->otz) * rsin(ang)) >> 12;
        block->dy = ((((s16)arg2 * 31) / block->otz) * rcos(ang)) >> 12;
        prim->x0  = block->sx + (u16)block->dx;
        prim->x3  = block->sx - (u16)block->dx;
        prim->y0  = block->sy - (u16)block->dy;
        prim->y3  = block->sy + (u16)block->dy;
        ang2      = ang + 0x400;
        block->dx = ((((s16)arg2 * 31) / block->otz) * rsin(ang2)) >> 12;
        block->dy = ((((s16)arg2 * 31) / block->otz) * rcos(ang2)) >> 12;
        prim->x1  = block->sx + (u16)block->dx;
        prim->x2  = block->sx - (u16)block->dx;
        prim->y1  = block->sy - (u16)block->dy;
        prim->y2  = block->sy + (u16)block->dy;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                prim);
    }
    SCRATCH_POP_BYTES_AT(scratch, 0x1C);
}

/// Draws an upright sprite at the coordinate's world position. The position is
/// projected through `GsWSMATRIX`; if the projection is valid, one
/// semi-transparent, unshaded `POLY_FT4` (tpage 0x2B, clut 0x43D2) is queued
/// as an axis-aligned square of half-side `r = (s16)arg2 * 55 / otz`, raised
/// so the projected point sits three quarters of the way down it. `arg1` picks
/// one of eight 56-texel frames in a grid four wide, starting at v 0x70.
static void func_shelter_b4_lower_sewer_8017FC14(GfxCoord* arg0, s32 arg1, s32 arg2)
{
    GpRingScratch* block;
    POLY_FT4*      prim;
    u16            idx;
    u32            cell;
    s32            row;
    u8             u0;
    u8             u1;
    u8             v0;
    u8             v1;

    idx           = arg1;
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
        prim           = gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2F);
        prim->tpage = 0x2B;
        prim->clut  = 0x43D2;
        cell        = idx;
        u0          = (cell & 3) * 0x38;
        row         = ((cell & 7) >> 2) * 0x38;
        v0          = row + 0x70;
        v1          = row + 0xA7;
        u1          = u0 + 0x37;
        setUV4(prim, u0, v0, u1, v0, u0, v1, u1, v1);
        block->step = ((s16)arg2 * 55) / block->otz;
        prim->x0 = prim->x2 = block->sx - block->step;
        prim->x1 = prim->x3 = block->sx + block->step;
        prim->y0 = prim->y1 = block->sy - block->step - (block->step >> 1);
        prim->y2 = prim->y3 = block->sy + (block->step >> 1);
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                prim);
    }
    SCRATCH_POP(GpRingScratch);
}

/// Per-frame driver of a burst of light in red, half blue and quarter green.
/// Over the number of frames given by the spawn argument the burst's
/// brightness `scale` and size `angle` grow together, drawn as a glow at that
/// size, a dimmer glow at twice it and a ring closing in around them. At the
/// peak the screen is flashed in the burst's colour, and the burst then fades
/// out through a larger star-shaped flare, shrinking by 8 and dimming by 0x10
/// a frame until its brightness drops to 0x10. The work block is then
/// released, as it is once the room's event state reaches 4; from event state
/// 1 on the burst is no longer advanced or drawn.
void func_shelter_b4_lower_sewer_8017FEB0(Task* task)
{
    GpEffWork* work;
    GfxCoord*  coord;
    u8         rgb[3];

    work  = task->spawnArg2.pointer;
    coord = task->extra.coordBody->coord;
    if (Gp_State1C->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
        if (Gp_State1C->effectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN) {
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
                func_shelter_b4_lower_sewer_80180580(coord, work->angle, rgb);
                rgb[0] >>= 1;
                rgb[1] >>= 1;
                rgb[2] >>= 1;
                func_shelter_b4_lower_sewer_80180580(coord, (s16)((u16)work->angle * 2), rgb);
                func_shelter_b4_lower_sewer_80180154(coord, (s16)(0x300 - (u16)work->angle * 2), 0x80, rgb);
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
                    func_shelter_b4_lower_sewer_80181484(coord, (s16)(work->angle * 3), rgb);
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

/// Draws a ring around the coordinate's world position. The position is
/// projected through `GsWSMATRIX`; if the projection is valid, sixteen gouraud
/// `POLY_G4` segments are queued between on-screen radii `(s16)arg1 * 64 /
/// (otz + 1)` and `(s16)(arg1 + arg2) * 64 / (otz + 1)`, black at the first
/// and coloured `rgb` at the second.
static void func_shelter_b4_lower_sewer_80180154(GfxCoord* arg0, s32 arg1, s32 arg2, u8* rgb)
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

/// Draws a round glow at the coordinate's world position. The position is
/// projected through `GsWSMATRIX`; if the projection is valid, eight gouraud
/// `POLY_G4` wedges of on-screen radius `(s16)arg1 * 64 / (otz + 1)` are
/// queued around the projected point, coloured `rgb` at the centre and black
/// at the rim.
static void func_shelter_b4_lower_sewer_80180580(GfxCoord* arg0, s16 arg1, u8* rgb)
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
    SCRATCH_POP(RoomFanScratch);
}

/// Per-frame driver of a ribbon trail swept by two points of a moving parent.
/// The first frame allocates two eight-slot histories of coordinates, places
/// the task's own coordinate at the first point under the parent and fills
/// every slot with the two points' current positions. Each later frame records
/// the two positions in the slot the age counter selects and draws the ribbon
/// through `func_shelter_b4_lower_sewer_80180E04`. The work block is released
/// once the age reaches the spawn argument; from the room's event state 2 on
/// the effect is frozen.
void func_shelter_b4_lower_sewer_80180914(Task* task)
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

    if (Gp_State1C->effectControl < ROOM_EFFECT_CONTROL_HIDDEN) {
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
                objCoord->coord.t[0]   = D_shelter_b4_lower_sewer_80181F94[0].vx;
                objCoord->coord.t[1]   = D_shelter_b4_lower_sewer_80181F94[0].vy;
                objCoord->coord.t[2]   = D_shelter_b4_lower_sewer_80181F94[0].vz;
                objCoord->composeStamp = GRAPHICS_COORD_DIRTY;
                Gp_UpdateCoord(objCoord);
                task->state        = 1;
                coord.parent       = work->parent;
                vec                = &D_shelter_b4_lower_sewer_80181F94[1];
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
                    SVECTOR* edge    = &D_shelter_b4_lower_sewer_80181F94[1];
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
                func_shelter_b4_lower_sewer_80180E04(coords, &coords[8], work->age & 7, 0x123);
                if (work->age == task->spawnArg1.value && work->age != 0) {
                    Gp_ReleaseState1CMem(work, task);
                }
                break;
        }
    }
}

/// Draws the ribbon between two eight-slot coordinate histories `arg0` and
/// `arg1` as seven gouraud `POLY_G4` quads, walking back from the newest slot
/// `arg2`; each quad joins the positions of two consecutive slots in both
/// histories and is skipped when its projection is invalid. The ribbon fades
/// from intensity 0x40 at the newest slot by 9 per slot. `arg3` packs the
/// colour: the red factor in bits 8 up and the green and blue factors in bits
/// 4-5 and 0-1, each multiplying that intensity.
static void func_shelter_b4_lower_sewer_80180E04(GfxCoord* arg0, GfxCoord* arg1, s16 arg2, s16 arg3)
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
        blk->v[0].vx = a->workm.t[0];
        j            = j - 1;
        blk->v[0].vy = a->workm.t[1];
        i1           = j & 7;
        blk->v[0].vz = a->workm.t[2];
        b            = &arg1[i0];
        blk->v[1].vx = b->workm.t[0];
        blk->v[1].vy = b->workm.t[1];
        blk->v[1].vz = b->workm.t[2];
        a            = &arg0[i1];
        blk->v[2].vx = a->workm.t[0];
        blk->v[2].vy = a->workm.t[1];
        blk->v[2].vz = a->workm.t[2];
        b            = &arg1[i1];
        blk->v[3].vx = b->workm.t[0];
        blk->v[3].vy = b->workm.t[1];
        blk->v[3].vz = b->workm.t[2];
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

/// Per-frame driver of an explosion at the task's coordinate. The first frame
/// spawns effect 0x60076 and then either, with a non-zero spawn argument,
/// effect 0x60070 and a spray of further 0x60070 sparks thrown at random
/// velocities over the next frames, or two 0x6007C effects and two expanding
/// rings drawn through `func_shelter_b4_lower_sewer_80180154` in a fading
/// orange. The work block is released once the age reaches 7, or when the
/// room's event state reaches 4; from event state 1 on nothing is advanced or
/// drawn.
void func_shelter_b4_lower_sewer_801811FC(Task* task)
{
    GfxCoord*  objCoord;
    GpEffWork* work;
    u8         rgb[4];

    objCoord = task->extra.coordBody->coord;
    work     = (GpEffWork*)task->spawnArg2.pointer;

    if (Gp_State1C->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
        if (Gp_State1C->effectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN) {
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
            func_shelter_b4_lower_sewer_80180154(objCoord, 0x100, 0x100, rgb);
            func_shelter_b4_lower_sewer_80180154(objCoord, work->scale, work->scale, rgb);
            if (work->age >= 7) {
                task->state = 3;
            }
            break;

        case 3:
            Gp_ReleaseState1CMem(work, task);
            break;
    }
}

/// Draws a star-shaped flare at the coordinate's world position. The position
/// is projected through `GsWSMATRIX`; if the projection is valid, gouraud
/// `POLY_G4` wedges are queued around the projected point: a glow of on-screen
/// radius `r = arg1 * 64 / (otz + 1)` at half the colour `arg2`, a glow of
/// radius `r / 2` at the full colour, and four spikes reaching out to `2 * r`
/// at half the colour. Every wedge fades from its colour at the centre to
/// black.
static void func_shelter_b4_lower_sewer_80181484(GfxCoord* arg0, s16 arg1, u8* arg2)
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
