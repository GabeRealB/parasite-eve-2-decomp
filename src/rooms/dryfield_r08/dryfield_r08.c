#include "rooms/dryfield_r08.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "gte.h"
#include "types.h"

#include "actors/task_tables.h"

#include "gameplay/display.h"
#include "gameplay/area.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/direction_input.h"
#include "gameplay/effects.h"
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
#include "main/gamemain.h"
#include "main/gfx.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "mapui/map_dryfield.h"

#include "rooms/room.h"

#include "rooms/room_common.h"
/// This room's `glowDrawDisc` stores the on-screen half-extent ahead of the
/// GTE flag word. Defined before `glow_draw.h`, which otherwise selects
/// `GlowCentreScratch`.
#define GLOW_DRAW_DISC_SCRATCH GlowCentreRadiusFirstScratch
#include "../../shared/glow_draw.h"

extern SVECTOR D_dryfield_r08_8017F464[];
extern SVECTOR D_dryfield_r08_8017F4C4[];
extern s32     D_dryfield_r08_80180C24;

extern GpRoomCoordSet D_dryfield_r08_801809C0;
extern GpRoomCoordSet D_dryfield_r08_80180B58;

static void func_dryfield_r08_8017DEFC(GfxCoord* arg0, u16 arg1, s16 arg2, s16 arg3);
static void func_dryfield_r08_8017E36C(GfxCoord* arg0, u16 arg1, s16 arg2, s16 arg3);
static void func_dryfield_r08_8017EB68(SVECTOR* arg0, s32 arg1, s32 arg2);

extern GpGridParams D_dryfield_r08_8017FB98[1];

extern GpAreaTmdRec D_dryfield_r08_80180B70[2];

SVECTOR D_dryfield_r08_8017F464[12] = {
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
};

SVECTOR D_dryfield_r08_8017F4C4[67] = {
    { 3360, 200, 3525, 0 },
    { 3880, 200, 3525, 0 },
    { 4600, 200, 3525, 0 },
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
    { 4365, -995, 335, 0 },
    { 8560, -1980, 2310, 0 },
    { 8560, -1980, 1925, 0 },
    { 9990, -2300, -5940, 0 },
    { 0x2887, -2300, -5940, 0 },
    { 0x27A1, -2300, -45, 0 },
    { 0x291D, -2300, -45, 0 },
    { 8915, -2720, -4090, 0 },
    { 8915, -2720, -3605, 0 },
    { 8915, -2720, -2125, 0 },
    { 8915, -2720, -1640, 0 },
    { 0x2D50, -2720, -4090, 0 },
    { 0x2D50, -2720, -3605, 0 },
    { 0x2D50, -2720, -2125, 0 },
    { 0x2D50, -2720, -1640, 0 },
};

GpRoomObjRec D_dryfield_r08_8017F6DC[2] = {
    { D_dryfield_r08_8017FB98, NULL, NULL, NULL },
    { D_dryfield_r08_8017FB98, NULL, NULL, NULL },
};

u8* D_dryfield_r08_8017F6FC[2] = {
    D_8010CAF8,
    D_8010CAF8,
};

GpViewCountRec D_dryfield_r08_8017F704[2] = {
    { { .bytes = { 6, 0 } } },
    { { .bytes = { 6, 0 } } },
};

GpRoomCoordRec D_dryfield_r08_8017F708[2] = {
    { &D_dryfield_r08_801809C0, NULL },
    { &D_dryfield_r08_80180B58, NULL },
};

GpWarpRec D_dryfield_r08_8017F718[1] = {
    { { .words = { 1024, 3360, 0, 2976 } }, { 0, 0, 0, 0 }, { .words = { 1024, 3360, 0, 2976 } }, { 0, 0, 0, 0 }, 0, 0, 0, 2, 0, 0 },
};

SVECTOR D_dryfield_r08_8017F750[6] = {
#include "assets/dryfield_r08_collision_025D8_normals.inc"
};

SVECTOR D_dryfield_r08_8017F780[59] = {
#include "assets/dryfield_r08_collision_025D8_verts.inc"
};

GpGridFace D_dryfield_r08_8017F958[31] = {
#include "assets/dryfield_r08_collision_025D8_faces.inc"
};

s16 D_dryfield_r08_8017FACC[86] = {
#include "assets/dryfield_r08_collision_025D8_cells.inc"
};

#define GRID_CELL(i) (&D_dryfield_r08_8017FACC[i])
s16* D_dryfield_r08_8017FB78[8] = {
#include "assets/dryfield_r08_collision_025D8_table.inc"
};
#undef GRID_CELL

GpGridParams D_dryfield_r08_8017FB98[1] = {
    { NULL, D_dryfield_r08_8017F750, D_dryfield_r08_8017F780, D_dryfield_r08_8017F958, D_dryfield_r08_8017FB78, 500, -500, 4, 2, 4000, 31 },
};

GpViewRec D_dryfield_r08_8017FBBC[6] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { -5500, 0x5208, -3500 } }, 380 },
    { { { { 108, 0, 4094 }, { 3896, 1258, -103 }, { -1257, 3897, 33 } }, { -5020, 1757, -3014 } }, 230 },
    { { { { 4095, 0, 0 }, { 0, 4017, -796 }, { 0, 796, 4017 } }, { -4888, 1408, -1622 } }, 261 },
    { { { { 15, 0, -4095 }, { 3773, 1592, 14 }, { 1592, -3773, 5 } }, { -4755, 952, -2617 } }, 257 },
    { { { { 3327, 0, 2388 }, { 304, 4062, -423 }, { -2368, 521, 3300 } }, { -7528, 1726, -67 } }, 415 },
    { { { { 108, 0, 4094 }, { 3896, 1258, -103 }, { -1257, 3897, 33 } }, { -5020, 1757, -3014 } }, 230 },
};

SpriteBatch D_dryfield_r08_8017FC94[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_r08_8017FCA4[85] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -72, 48, 249, { .fields = { 56, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 64, 48, 250, { .fields = { 32, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 72, 64, 243, { .fields = { 32, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 72, 96, 227, { .fields = { 40, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 24 } }, -80, 24, 238, { .fields = { 0, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, 40, 16, 215, { .fields = { 32, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 56, 24, 238, { .fields = { 40, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, 16, 16, 204, { .fields = { 16, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 32 } }, -80, 72, 222, { .fields = { 80, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 32 } }, -56, 72, 202, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 32 } }, -32, 72, 190, { .fields = { 48, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 32 } }, -8, 72, 189, { .fields = { 48, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 32 } }, 16, 72, 190, { .fields = { 56, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 32 } }, 40, 72, 201, { .fields = { 72, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, 64, 72, 227, { .fields = { 0, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -48, 16, 207, { .fields = { 72, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -32, 16, 207, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -16, 16, 201, { .fields = { 8, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 0, 16, 201, { .fields = { 8, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -80, -112, 314, { .fields = { 88, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -80, -56, 302, { .fields = { 64, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 96, -32, 454, { .fields = { 64, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -72, -16, 471, { .fields = { 88, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -96, -104, 519, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 32 } }, -8, -120, 320, { .fields = { 88, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, 16, -120, 322, { .fields = { 16, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 48 } }, 16, -40, 255, { .fields = { 96, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 32 } }, 88, 72, 400, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 32, 16, 212, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -8, -88, 522, { .fields = { 104, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -104, -88, 313, { .fields = { 96, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -32, 16, 205, { .fields = { 72, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 48 } }, 64, -32, 466, { .fields = { 64, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 32 } }, -136, -48, 274, { .fields = { 32, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 32 } }, -112, -48, 274, { .fields = { 24, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 32 } }, -88, -48, 276, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, -64, -24, 323, { .fields = { 32, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -56, -32, 254, { .fields = { 32, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -32, -32, 254, { .fields = { 8, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 32 } }, -72, 88, 224, { .fields = { 24, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 32 } }, 112, -72, 275, { .fields = { 56, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -64, -48, 256, { .fields = { 0, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -32, -48, 256, { .fields = { 32, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -160, 40, 252, { .fields = { 0, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -160, 80, 240, { .fields = { 0, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -128, 40, 252, { .fields = { 32, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -128, 80, 240, { .fields = { 32, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 32 } }, -96, 88, 240, { .fields = { 24, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -96, 48, 255, { .fields = { 96, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 32 } }, -96, 56, 246, { .fields = { 8, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 40 } }, -72, 48, 250, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 32 } }, -72, 16, 250, { .fields = { 64, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 40 } }, -40, -88, 310, { .fields = { 96, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 32 } }, -40, -120, 310, { .fields = { 64, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 32 } }, -72, -80, 310, { .fields = { 72, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 40 } }, -72, -120, 310, { .fields = { 104, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, 24, -72, 260, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, 48, -72, 260, { .fields = { 104, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 56 } }, 48, 16, 251, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 56 } }, 72, 16, 251, { .fields = { 104, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 24, -120, 295, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 48, -120, 295, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 72, -120, 293, { .fields = { 104, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 96, -120, 293, { .fields = { 104, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 96, -24, 230, { .fields = { 88, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 120, -24, 230, { .fields = { 112, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 40 } }, 96, 24, 219, { .fields = { 112, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 120, 24, 219, { .fields = { 120, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 96, 64, 214, { .fields = { 40, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 120, 64, 214, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, 72, -72, 273, { .fields = { 8, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 96, -72, 273, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 24, -32, 254, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 40, -32, 254, { .fields = { 64, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 32 } }, -96, 16, 267, { .fields = { 96, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 32 } }, -96, -16, 267, { .fields = { 48, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, 48, 72, 227, { .fields = { 56, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 48 } }, 72, 72, 226, { .fields = { 64, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -8, -80, 258, { .fields = { 8, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 32 } }, -120, -16, 273, { .fields = { 8, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -120, 16, 262, { .fields = { 40, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 24 } }, -144, 16, 263, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 32 } }, -144, -16, 265, { .fields = { 32, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -40, 8, 252, { .fields = { 64, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 24, 8 } }, -64, 8, 252, { .fields = { 88, 248 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_r08_80180348[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 19, 0, 0, { 1, 0 } },
    { 19, 66, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_r08_80180368[12] = {
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -24, 96, 204, { .fields = { 96, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 16, 24, 234, { .fields = { 104, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 24, 96, 204, { .fields = { 96, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -16, 24, 240, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 80, 24, 279, { .fields = { 96, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 88, 40, 239, { .fields = { 96, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 104, 64, 233, { .fields = { 104, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 104, 88, 228, { .fields = { 96, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, 24, 64, 223, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 24, 40, 246, { .fields = { 104, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, -16, 40, 246, { .fields = { 104, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -16, 64, 224, { .fields = { 104, 32 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_r08_80180458[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 12, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_r08_80180470[14] = {
    { 143, 0x3FC0, { .fields = { 32, 48 } }, 16, -72, 199, { .fields = { 96, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 40 } }, -16, -64, 199, { .fields = { 56, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 40 } }, -16, 0, 222, { .fields = { 32, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 40 } }, -40, 40, 286, { .fields = { 40, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, -88, 48, 244, { .fields = { 88, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 40 } }, -152, 40, 238, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -160, 0, 286, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 56 } }, -160, -120, 188, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 32 } }, -120, -120, 184, { .fields = { 48, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 56 } }, -56, -120, 187, { .fields = { 88, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 40 } }, -16, -120, 187, { .fields = { 72, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 32 } }, -112, -80, 194, { .fields = { 24, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 24 } }, -104, -48, 194, { .fields = { 48, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, -97, 0, 242, { .fields = { 88, 208 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_r08_80180588[13] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 2, 0, 0, { 1, 0 } },
    { 2, 1, 0, 0, { 6, 0 } },
    { 3, 1, 0, 0, { 2, 0 } },
    { 4, 1, 0, 0, { 7, 0 } },
    { 5, 1, 0, 0, { 5, 0 } },
    { 6, 1, 0, 0, { 8, 0 } },
    { 7, 1, 0, 0, { 4, 0 } },
    { 8, 1, 0, 0, { 9, 0 } },
    { 9, 2, 0, 0, { 0, 0 } },
    { 11, 2, 0, 0, { 10, 0 } },
    { 13, 1, 0, 0, { 3, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_r08_801805F0[8] = {
    { 143, 0x3FC0, { .fields = { 8, 8 } }, 104, 112, 977, { .fields = { 120, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 96 } }, 24, 24, 1250, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 104 } }, 80, 16, 1250, { .fields = { 112, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, 96, 16, 1250, { .fields = { 40, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 48 } }, -136, 24, 1250, { .fields = { 16, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 48 } }, -136, 72, 1250, { .fields = { 32, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 56, 104 } }, -96, 16, 1250, { .fields = { 56, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 120 } }, -40, 0, 1250, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_r08_80180690[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 8, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_dryfield_r08_801806A8[30] = {
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -80, -112, 314, { .fields = { 104, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, 8, -96, 530, { .fields = { 104, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -96, -72, 510, { .fields = { 104, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -80, -56, 302, { .fields = { 48, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -64, -8, 465, { .fields = { 48, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 8 } }, -64, 8, 262, { .fields = { 104, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, -104, -88, 313, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 72 } }, 88, -24, 228, { .fields = { 56, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 72, 72 } }, -144, -48, 268, { .fields = { 32, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 40 } }, -152, -16, 272, { .fields = { 16, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 32 } }, -96, -104, 328, { .fields = { 8, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -88, -72, 303, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 144, 128 } }, -56, -8, 237, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 80, 112 } }, -72, -120, 258, { .fields = { 48, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 96, 96 } }, 24, -120, 282, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -72, 16, 245, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -72, 88, 223, { .fields = { 0, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -72, 48, 234, { .fields = { 16, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 8, -24, 254, { .fields = { 88, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 32, 16 } }, 24, -24, 255, { .fields = { 72, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 56, -24, 337, { .fields = { 56, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, 8, -80, 258, { .fields = { 120, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 8, -48, 256, { .fields = { 0, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, 8, -120, 315, { .fields = { 0, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 64 } }, 96, 48, 210, { .fields = { 8, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 56 } }, 88, 48, 240, { .fields = { 24, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 72, 96 } }, -144, 24, 248, { .fields = { 104, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -160, 24, 259, { .fields = { 0, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -160, 56, 248, { .fields = { 32, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -160, 88, 239, { .fields = { 104, 224 } }, 128, 128, 128, 0 },
};

SpriteBatch D_dryfield_r08_80180900[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 30, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

GpSprtRec D_dryfield_r08_80180918[6] = {
    { { .empty = D_dryfield_r08_8017FC94 }, D_dryfield_r08_8017FC94, NULL },
    { { .elements = D_dryfield_r08_8017FCA4 }, D_dryfield_r08_80180348, NULL },
    { { .elements = D_dryfield_r08_80180368 }, D_dryfield_r08_80180458, NULL },
    { { .elements = D_dryfield_r08_80180470 }, D_dryfield_r08_80180588, NULL },
    { { .elements = D_dryfield_r08_801805F0 }, D_dryfield_r08_80180690, NULL },
    { { .elements = D_dryfield_r08_801806A8 }, D_dryfield_r08_80180900, NULL },
};

WorldCoordPointLight D_dryfield_r08_80180960[1] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 5215, -1041, 3017 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 0x3000, 0x3000, 0x3000 }, { 0, 0 } }, 381, 1500 },
};

GpRoomCoordSet D_dryfield_r08_801809C0 = { 0, NULL, 1, D_dryfield_r08_80180960, 0, NULL };

WorldCoordPointLight D_dryfield_r08_801809D8[4] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 5781, 225, 2364 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3440, 4096, 4096 }, { 0, 0 } }, 400, 1150 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 4644, 204, 2683 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3440, 4096, 4096 }, { 0, 0 } }, 400, 1000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 6730, -2042, 2920 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 409, 163 }, { 0, 0 } }, 0, 2200 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 3860, -2042, 2980 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 409, 163 }, { 0, 0 } }, 0, 2700 },
};

GpRoomCoordSet D_dryfield_r08_80180B58 = { 0, NULL, 4, D_dryfield_r08_801809D8, 0, NULL };

GpAreaTmdRec D_dryfield_r08_80180B70[2] = {
    { 132, 213, 0, 0, { 0, 0 }, D_8013D390 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaVariant D_dryfield_r08_80180B88[13] = {
    { NULL, NULL },
    { D_map_dryfield_8017AF44, D_dryfield_r08_80180B70 },
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

s32 D_dryfield_r08_80180BF0[3] = {
    0x10000011,
    0x10000013,
    0x10000011,
};

GpRoomParamRec D_dryfield_r08_80180BFC[1] = {
    { 0, 0, 1, 0, NULL },
};

GpRoomParamRec* D_dryfield_r08_80180C04[8] = {
    D_dryfield_r08_80180BFC,
    D_dryfield_r08_80180BFC,
    D_dryfield_r08_80180BFC,
    D_dryfield_r08_80180BFC,
    D_dryfield_r08_80180BFC,
    D_dryfield_r08_80180BFC,
    D_dryfield_r08_80180BFC,
    D_dryfield_r08_80180BFC,
};

s32 D_dryfield_r08_80180C24 = 0;

static void func_dryfield_r08_8017F3B8(u8 arg0, u8 arg1);

void func_dryfield_r08_8017D5F8(Task* task)
{
    s32 i;
    u8  view;

    if (task->state == 0) {
        D_dryfield_r08_80180C24 = 0;
        task->state             = task->state + 1;
    }

    view = Gp_GetViewIndex();
    switch (view) {
        case 2: {
            SVECTOR* q;

            q = D_dryfield_r08_8017F4C4;
            glowDrawDisc(&q[0], 0x200, 0x444);
            glowDrawDisc(&q[2], 0x200, 0x444);
            glowDrawDisc(&q[3], 0x200, 0x444);
            glowDrawDisc(&q[6], 0x200, 0x444);
            glowDrawDisc(&q[14], 0x200, 0x444);
            glowDrawDisc(&q[15], 0x200, 0x444);
            glowDrawDisc(&q[16], 0x200, 0x444);
            glowDrawDisc(&q[17], 0x200, 0x444);
            break;
        }
        case 3:
            for (i = D_dryfield_r08_80180C24; i < 12; i++) {
                func_dryfield_r08_8017EB68(&D_dryfield_r08_8017F464[i], 0xA0, 0x3888);
            }
            break;
        case 4:
            for (i = D_dryfield_r08_80180C24; i < 12; i++) {
                func_dryfield_r08_8017EB68(&D_dryfield_r08_8017F464[i], 0xA0, 0x3888);
            }
            break;
        case 5: {
            SVECTOR* q;

            q = D_dryfield_r08_8017F4C4;
            glowDrawDisc(&q[0], 0x200, 0x444);
            glowDrawDisc(&q[1], 0x200, 0x444);
            glowDrawDisc(&q[2], 0x200, 0x444);
            glowDrawDisc(&q[7], 0x200, 0x444);
            glowDrawDisc(&q[8], 0x200, 0x444);
            glowDrawDisc(&q[21], 0x200, 0x400);
            glowDrawDisc(&q[22], 0x200, 0x400);
            glowDrawDisc(&q[23], 0x200, 0x400);
            glowDrawDisc(&q[29], 0x200, 0x400);
            break;
        }
        case 6: {
            SVECTOR* q;

            q = D_dryfield_r08_8017F4C4;
            glowDrawDisc(&q[0], 0x200, 0x433);
            glowDrawDisc(&q[2], 0x200, 0x433);
            glowDrawDisc(&q[3], 0x200, 0x433);
            glowDrawDisc(&q[6], 0x200, 0x433);
            glowDrawDisc(&q[14], 0x200, 0x433);
            glowDrawDisc(&q[15], 0x200, 0x433);
            glowDrawDisc(&q[16], 0x200, 0x433);
            glowDrawDisc(&q[17], 0x200, 0x433);
            break;
        }
    }
}

/// Per-frame handler for one animated sprite effect, drawn by
/// `func_dryfield_r08_8017DEFC` or `func_dryfield_r08_8017E36C`. Its first
/// frame unpacks `spawnArg1`: the low 12 bits are the sprite size, bits 12..14
/// the frames per animation cell (1 when zero), bits 28..30 are kept as the
/// drawer's clut selector, and the sign bit picks the second drawer. When the
/// work block arrives without a velocity, bits 24..27 choose how one is rolled
/// from `Gp_LcgState` (0 leaves it still) and it
/// is normalised to a speed from bits 16..23 (0x40 when zero). Each later frame
/// draws the current cell, moves the coordinate by the velocity and bends its
/// Y component, then frees the effect after the drawer's last cell (12 or 10).
/// While the player is in an event it only draws, and frees once the event
/// aborts.
void func_dryfield_r08_8017D8B4(Task* task)
{
    EffectWork* work;
    GfxCoord*   coord;
    SVECTOR*    vec;
    s32         step;
    s32         level;

    work  = task->spawnArg2.pointer;
    coord = task->extra.coordBody->coord;
    if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
        func_dryfield_r08_8017DEFC(coord, work->index, work->scale, work->angle);
        if (gRoomEffectState->effectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            Gp_ReleaseState1CMem(work, task);
        }
        return;
    }
    work->age++;
    switch (task->state) {
        case 0:
            work->scale = task->spawnArg1.value & 0xFFF;
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            work->angle = ((u32)Gp_LcgState >> 16) & 0xFFF;
            if (task->spawnArg1.value & 0xF000) {
                step = (task->spawnArg1.value >> 12) & 7;
            } else {
                step = 1;
            }
            work->period = step;
            work->age    = 0;
            task->state  = 1;
            task->state  = task->spawnArg1.value < 0 ? 2 : 1;
            work->pos.vx = (task->spawnArg1.value >> 16) & 0x7000;
            if ((work->move.vx | work->move.vy | work->move.vz) == 0) {
                if (task->spawnArg1.value & 0xFF0000) {
                    level = (task->spawnArg1.value >> 16) & 0xFF;
                } else {
                    level = 0x40;
                }
                work->step = level;
                switch ((task->spawnArg1.value >> 24) & 0xF) {
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
                    case 6:
                        work->move.vy = 0;
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vx = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                        work->move.vz = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
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
            break;
        case 1:
            func_dryfield_r08_8017DEFC(coord, work->index | work->pos.vx, work->scale, work->angle);
            if (work->step != 0) {
                coord->coord.t[0]  += work->move.vx;
                coord->coord.t[1]  += work->move.vy;
                coord->coord.t[2]  += work->move.vz;
                coord->composeStamp = GRAPHICS_COORD_DIRTY;
                if (((task->spawnArg1.value >> 24) & 0xF) == 7) {
                    work->move.vy += work->age / 10;
                } else {
                    work->move.vy -= 2;
                }
            }
            if ((work->age % work->period) == 0) {
                work->index++;
                if (work->index >= 12) {
                    Gp_ReleaseState1CMem(work, task);
                }
            }
            break;
        case 2:
            func_dryfield_r08_8017E36C(coord, work->index | work->pos.vx, work->scale, work->angle);
            if (work->step != 0) {
                coord->coord.t[0]  += work->move.vx;
                coord->coord.t[1]  += work->move.vy;
                coord->coord.t[2]  += work->move.vz;
                coord->composeStamp = GRAPHICS_COORD_DIRTY;
                if (((task->spawnArg1.value >> 24) & 0xF) == 7) {
                    work->move.vy += work->age / 10;
                } else {
                    work->move.vy -= 1;
                }
            }
            if ((work->age % work->period) == 0) {
                work->index++;
                if (work->index >= 10) {
                    Gp_ReleaseState1CMem(work, task);
                }
            }
            break;
    }
}

/// Same projected, spinning `POLY_FT4` as `func_dryfield_r08_8017E36C`, with
/// its own texture window: the low 12 bits of `arg1` pick a 48x48 cell from a
/// five-column grid (u = `cell % 5 * 48`, v = `cell / 5 * 48 + 0x68`), and the
/// top four bits select the clut - row `0x10E + sel` at column `cell & 0x3F`
/// for 0 and 1, the fixed clut 0x428F otherwise.
static void func_dryfield_r08_8017DEFC(GfxCoord* arg0, u16 arg1, s16 arg2, s16 arg3)
{
    GpEffFlareScratch* block;
    POLY_FT4*          prim;
    s32                ang;
    s32                ang2;
    s32                span;
    s32                u0;
    s32                v0;
    s32                u1;
    s32                v1;
    s32                tex;
    u16                sel;
    s32                sine;

    block         = SCRATCH_STACK_RESERVE_BLOCK(GpEffFlareScratch);
    block->vec.vx = arg0->workm.t[0];
    block->vec.vy = arg0->workm.t[1];
    block->vec.vz = arg0->workm.t[2];
    tex           = arg1 & 0xFFF;
    sel           = arg1 >> 12;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps();
    prim           = gGpuPrimCursor;
    gGpuPrimCursor = prim + 1;
    setlen(prim, 9);
    setcode(prim, 0x2C);
    gte_stsxy(&block->sx);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        prim->tpage = 0x2B;
        prim->code |= 3;
        if (sel >= 2) {
            prim->clut = 0x428F;
        } else {
            prim->clut = ((sel + 0x10E) << 6) | (tex & 0x3F);
        }
        ang = arg3;
        u0  = ((u16)tex % 5) * 0x30;
        v0  = ((u16)tex / 5) * 0x30;
        u1  = u0 + 0x2F;
        v1  = v0 - 0x69;
        v0  = v0 + 0x68;
        setUV4(prim, u0, v0, u1, v0, u0, v1, u1, v1);
        sine      = rsin(ang);
        span      = arg2 * 0x2F;
        block->dx = ((span / block->otz) * sine) >> 12;
        block->dy = ((span / block->otz) * rcos(ang)) >> 12;
        prim->x0  = block->sx + (u16)block->dx;
        prim->x3  = block->sx - (u16)block->dx;
        prim->y0  = block->sy - (u16)block->dy;
        prim->y3  = block->sy + (u16)block->dy;
        ang2      = ang + 0x400;
        block->dx = ((span / block->otz) * rsin(ang2)) >> 12;
        block->dy = ((span / block->otz) * rcos(ang2)) >> 12;
        prim->x1  = block->sx + (u16)block->dx;
        prim->x2  = block->sx - (u16)block->dx;
        prim->y1  = block->sy - (u16)block->dy;
        prim->y2  = block->sy + (u16)block->dy;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                prim);
    }
    SCRATCH_STACK_RELEASE_BLOCK(GpEffFlareScratch);
}

/// Projects `arg0`'s world translation through `GsWSMATRIX` and, unless the
/// GTE flag word is negative, queues one semi-transparent `POLY_FT4` centred on
/// the projected point. The low 12 bits of `arg1` pick a 48x48 cell from a
/// five-column grid (u = `cell % 5 * 48`, v = `cell / 5 * 48 - 0x80`); any of
/// its top four bits set selects clut 0x428F instead of 0x43D0. The quad's
/// diagonals are `arg2 * 47 / otz` long, turned by `arg3` and
/// `arg3 + 0x400`, so it shrinks with distance and spins with the angle.
static void func_dryfield_r08_8017E36C(GfxCoord* arg0, u16 arg1, s16 arg2, s16 arg3)
{
    GpEffFlareScratch* block;
    POLY_FT4*          prim;
    s32                ang;
    s32                ang2;
    s32                span;
    s32                u0;
    s32                v0;
    s32                u1;
    s32                v1;
    u16                tex;
    u16                sel;
    s32                sine;

    block         = SCRATCH_STACK_RESERVE_BLOCK(GpEffFlareScratch);
    block->vec.vx = arg0->workm.t[0];
    sel           = arg1 >> 12;
    block->vec.vy = arg0->workm.t[1];
    block->vec.vz = arg0->workm.t[2];
    tex           = arg1 & 0xFFF;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps();
    prim           = gGpuPrimCursor;
    gGpuPrimCursor = prim + 1;
    setlen(prim, 9);
    setcode(prim, 0x2C);
    gte_stsxy(&block->sx);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        prim->tpage = 0x2C;
        prim->code |= 3;
        if (sel) {
            prim->clut = 0x428F;
        } else {
            prim->clut = 0x43D0;
        }
        ang = arg3;
        u0  = (tex % 5) * 0x30;
        v0  = (tex / 5) * 0x30;
        u1  = u0 + 0x2F;
        v1  = v0 - 0x51;
        v0  = v0 - 0x80;
        setUV4(prim, u0, v0, u1, v0, u0, v1, u1, v1);
        sine      = rsin(ang);
        span      = arg2 * 0x2F;
        block->dx = ((span / block->otz) * sine) >> 12;
        block->dy = ((span / block->otz) * rcos(ang)) >> 12;
        prim->x0  = block->sx + block->dx;
        prim->x3  = block->sx - block->dx;
        prim->y0  = block->sy - block->dy;
        prim->y3  = block->sy + block->dy;
        ang2      = ang + 0x400;
        block->dx = ((span / block->otz) * rsin(ang2)) >> 12;
        block->dy = ((span / block->otz) * rcos(ang2)) >> 12;
        prim->x1  = block->sx + block->dx;
        prim->x2  = block->sx - block->dx;
        prim->y1  = block->sy - block->dy;
        prim->y2  = block->sy + block->dy;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                prim);
    }
    SCRATCH_STACK_RELEASE_BLOCK(GpEffFlareScratch);
}

#include "../../shared/glow_draw_disc.inc.c"

/// Projects the world-space point `arg0` through `gGfxViewCoord.workm` and, when
/// the GTE flag is non-negative, queues an eight-wedge gouraud disc plus four
/// inner cross wedges around the projected centre. `arg1` is a
/// signed half-extent; on-screen radii are `(s16)arg1 * 64 / otz` (outer) and
/// `(s16)arg1 * 8 / otz` (inner). `arg2` packs the colour one nibble per
/// channel - bits 8..11 red, 4..7 green, 0..3 blue, each scaled to 8 bits -
/// with bits 12..15 giving the shift for a `gDisplayState.animFrame & 1`
/// flicker added to every channel. The outer disc uses the full colour and
/// the inner cross half of it.
static void func_dryfield_r08_8017EB68(SVECTOR* arg0, s32 arg1, s32 arg2)
{
    RoomDiscScratch* block;
    POLY_G4*         prim;
    s32              ang;
    s32              t;
    s32              t2;
    s32              ua;
    s32              ub;
    s32              uc;
    s32              frame;
    s32              packed;
    s32              blend;
    s32              r;
    s32              g;
    s32              b;
    s32              outer;
    s32              inner;

    {
        void** scratch;
        u8*    tmp;

        scratch = SCRATCH_STACK_CURSOR_SLOT;
        tmp     = SCRATCH_PUSH_BYTES_AT(scratch, 0x14);
        block   = (RoomDiscScratch*)tmp;
    }

    gte_SetTransMatrix(&gGfxViewCoord.workm);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_ldv0(arg0);
    gte_rtps();
    gte_stsxy(&block->sx);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        arg1        <<= 16;
        arg1        >>= 16;
        outer         = (arg1 * 64) / block->otz;
        frame         = gDisplayState.animFrame;
        block->rOuter = outer;
        inner         = (arg1 * 8) / block->otz;
        ang           = 0;
        packed        = arg2 << 16;
        blend         = (frame & 1) << (packed >> 28);
        r             = blend + ((packed >> 20) & 0xF0);
        g             = blend + ((packed >> 16) & 0xF0);
        b             = blend + ((arg2 & 0xF) << 4);
        block->rInner = inner;
        do {
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, r, g, b);
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
            ang      = t2;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        } while (ang < 0x1000);

        r   = (u8)r >> 1;
        g   = (u8)g >> 1;
        b   = (u8)b >> 1;
        ang = 0x200;
        do {
            ua             = ang - 0x400;
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, r, g, b);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->rInner * rsin(ua)) >> 12);
            prim->y0 = block->sy + ((block->rInner * rcos(ua)) >> 12);
            prim->x1 = block->sx + ((block->rOuter * rsin(ang)) >> 12);
            prim->y1 = block->sy + ((block->rOuter * rcos(ang)) >> 12);
            ub       = ang + 0x400;
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->rInner * rsin(ub)) >> 12);
            prim->y3 = block->sy + ((block->rInner * rcos(ub)) >> 12);
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
            prim->x1 = block->sx + ((block->rOuter * rsin(ub)) >> 11);
            prim->y1 = block->sy + ((block->rOuter * rcos(ub)) >> 11);
            uc       = ang + 0x800;
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->rInner * rsin(uc)) >> 12);
            prim->y3 = block->sy + ((block->rInner * rcos(uc)) >> 12);
            ang      = uc;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        } while (ang < 0x1000);
    }
    SCRATCH_STACK_RELEASE_BYTES(0x14);
}

void func_dryfield_r08_8017F334(s32 arg0)
{
    D_dryfield_r08_80180C24 = arg0;
}

/// Sets the skip-OT-link byte (`SpriteBatch.hidden`) of command record
/// `arg0` + 1 in this room's sprite-table command list: non-zero leaves that
/// record's prims out of the ordering table. `arg0` is a view index below
/// 0xB; the record the table yields is larger than its `GpSprtRec` prefix,
/// so `[3].field_4` reaches the command list its tail holds there.
void func_dryfield_r08_8017F340(u8 arg0, u8 arg1)
{
    GameLocationKey* sess;
    SpriteBatch*     batches;

    sess = &gGameSession->location.loc;
    if ((u32)(arg0 & 0xFF) < 0xBU) {
        batches = Gp_SprtTables[sess->stage - 1]->field_0[sess->area - 1][3].field_4;
        if (arg1 & 0xFF) {
            batches[arg0 + 1].hidden = 1;
            return;
        }
        batches[arg0 + 1].hidden = 0;
    }
}

static void func_dryfield_r08_8017F3B8(u8 arg0, u8 arg1)
{
    GameLocationKey* sess;
    GpSprtRec*       rec;
    SpriteBatch*     batches;

    sess = &gGameSession->location.loc;
    if ((u32)(arg0 & 0xFF) < 3U) {
        rec = Gp_SprtTables[sess->stage - 1]->field_0[sess->area - 1];
        if ((u32)(arg0 & 0xFF) == 0U) {
            batches = rec[1].field_4;
        } else {
            batches = rec[2].field_4;
        }
        if (arg1 & 0xFF) {
            batches[1].hidden = 1;
            return;
        }
        batches[1].hidden = 0;
    }
}

/// Publishes one of the room's two data banks as the active one: bank 0 for a
/// zero argument, bank 1 otherwise.
void func_dryfield_r08_8017F438(s16 arg0)
{
    if (arg0 == 0) {
        D_dryfield_r08_8017F708[0].field_0 = &D_dryfield_r08_801809C0;
        return;
    }
    D_dryfield_r08_8017F708[0].field_0 = &D_dryfield_r08_80180B58;
}
