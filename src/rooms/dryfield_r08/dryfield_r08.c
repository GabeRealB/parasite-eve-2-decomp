#include "rooms/dryfield_r08.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "gte.h"
#include "common.h"

#include "actors/task_tables.h"

#include "gameplay/display.h"
#include "gameplay/area.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
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
#include "main/random.h"
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
#include "../../shared/effect_sprite.h"

extern SVECTOR D_dryfield_r08_8017F464[];
extern SVECTOR D_dryfield_r08_8017F4C4[];
extern s32     D_dryfield_r08_80180C24;

extern WorldCoordRoomLights D_dryfield_r08_801809C0;
extern WorldCoordRoomLights D_dryfield_r08_80180B58;

static void _dryfieldR08DrawBankedDriftSprite(const GfxCoord* coord, u16 frameAndPalette, s16 size, s16 angle);
static void _dryfieldR08DrawAlternateDriftSprite(const GfxCoord* coord, u16 frameAndPalette, s16 size, s16 angle);
static void func_dryfield_r08_8017EB68(SVECTOR* arg0, s32 arg1, s32 arg2);

extern WorldCollisionGrid D_dryfield_r08_8017FB98[1];

extern AreaResource D_dryfield_r08_80180B70[2];

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

WorldCollisionRoomResources D_dryfield_r08_8017F6DC[2] = {
    { D_dryfield_r08_8017FB98, NULL, NULL, NULL },
    { D_dryfield_r08_8017FB98, NULL, NULL, NULL },
};

u8* D_dryfield_r08_8017F6FC[2] = {
    gViewIdentityMap,
    gViewIdentityMap,
};

ViewCount D_dryfield_r08_8017F704[2] = { 6, 6 };

WorldCoordRoomLighting D_dryfield_r08_8017F708[2] = {
    { &D_dryfield_r08_801809C0, NULL },
    { &D_dryfield_r08_80180B58, NULL },
};

DirectionWarpEntry D_dryfield_r08_8017F718[1] = {
    { { { .word = 1024 }, 3360, 0, 2976 }, { 0, 0, 0, 0 }, { { .word = 1024 }, 3360, 0, 2976 }, { 0, 0, 0, 0 }, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
};

static SVECTOR _gDryfieldR08Collision025D8Normals[6] = {
#include "assets/dryfield_r08_collision_025D8_normals.inc"
};

static SVECTOR _gDryfieldR08Collision025D8Verts[59] = {
#include "assets/dryfield_r08_collision_025D8_verts.inc"
};

static WorldCollisionGridFace _gDryfieldR08Collision025D8Faces[31] = {
#include "assets/dryfield_r08_collision_025D8_faces.inc"
};

static s16 _gDryfieldR08Collision025D8Cells[86] = {
#include "assets/dryfield_r08_collision_025D8_cells.inc"
};

#define GRID_CELL(i) (&_gDryfieldR08Collision025D8Cells[i])
static s16* _gDryfieldR08Collision025D8Table[8] = {
#include "assets/dryfield_r08_collision_025D8_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid D_dryfield_r08_8017FB98[1] = {
    { NULL, _gDryfieldR08Collision025D8Normals, _gDryfieldR08Collision025D8Verts, _gDryfieldR08Collision025D8Faces, _gDryfieldR08Collision025D8Table, 500, -500, 4, 2, 4000, 31 },
};

ViewCamera D_dryfield_r08_8017FBBC[6] = {
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

SpriteView D_dryfield_r08_80180918[6] = {
    { { .empty = D_dryfield_r08_8017FC94 }, D_dryfield_r08_8017FC94, NULL },
    { { .elements = D_dryfield_r08_8017FCA4 }, D_dryfield_r08_80180348, NULL },
    { { .elements = D_dryfield_r08_80180368 }, D_dryfield_r08_80180458, NULL },
    { { .elements = D_dryfield_r08_80180470 }, D_dryfield_r08_80180588, NULL },
    { { .elements = D_dryfield_r08_801805F0 }, D_dryfield_r08_80180690, NULL },
    { { .elements = D_dryfield_r08_801806A8 }, D_dryfield_r08_80180900, NULL },
};

/// The default lighting bank's white point light, contributing in every room view.
///
/// Position and falloff radii use integer world units; RGB intensities use
/// 12 fractional bits, with each channel initially at three times `ONE`.
/// The loaded room overlay owns this mutable array: coordinate updates parent
/// and compose its transform, and lighting queries overwrite attenuation.
static WorldCoordPointLight _gDryfieldR08DefaultPointLights[] = {
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 5215, -1041, 3017 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { 3 * ONE, 3 * ONE, 3 * ONE },
        },
        .inner = 381,
        .outer = 1500,
    },
};

WorldCoordRoomLights D_dryfield_r08_801809C0 = { 0, NULL, ARRAY_SIZE(_gDryfieldR08DefaultPointLights), _gDryfieldR08DefaultPointLights, 0, NULL };

/// The alternate lighting bank's two cyan and two red point lights.
///
/// All four accept every room view. Positions and falloff radii use integer
/// world units; RGB intensities have 12 fractional bits. The loaded room
/// overlay owns this mutable array: coordinate updates parent and compose its
/// transforms, and lighting queries overwrite attenuation. Pointers must not
/// survive unloading the overlay.
static WorldCoordPointLight _gDryfieldR08AlternatePointLights[] = {
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 5781, 225, 2364 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { 3440, ONE, ONE },
        },
        .inner = 400,
        .outer = 1150,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 4644, 204, 2683 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { 3440, ONE, ONE },
        },
        .inner = 400,
        .outer = 1000,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 6730, -2042, 2920 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { ONE, 409, 163 },
        },
        .inner = 0,
        .outer = 2200,
    },
    {
        .head = {
            .transform = { .lighting = {
                               .composeStamp = GRAPHICS_COORD_DIRTY,
                               .local        = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 3860, -2042, 2980 } },
                               .composed     = { { { ONE, 0, 0 }, { 0, ONE, 0 }, { 0, 0, ONE } }, { 0, 0, 0 } },
                               .viewId       = WORLD_COORDINATE_LIGHT_ALL_VIEWS,
                               .parent       = NULL,
                           } },
            .color     = { ONE, 409, 163 },
        },
        .inner = 0,
        .outer = 2700,
    },
};

WorldCoordRoomLights D_dryfield_r08_80180B58 = { 0, NULL, ARRAY_SIZE(_gDryfieldR08AlternatePointLights), _gDryfieldR08AlternatePointLights, 0, NULL };

AreaResource D_dryfield_r08_80180B70[2] = {
    { 132, 213, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, D_actor_121300_8013D390 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaVariant D_dryfield_r08_80180B88[13] = {
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

WorldCollisionSurfaceProperties D_dryfield_r08_80180BFC[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties* D_dryfield_r08_80180C04[8] = {
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

    view = viewGetMappedIndex();
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

/// Writes the first drift-sprite corner offset and returns its perspective numerator.
///
/// `projection` borrows a live workspace with positive `depth`; only its corner
/// offsets change. The return is `sizeFactor * 47`, shared by both corner pairs;
/// dividing it by depth gives the signed pixel half-diagonal, truncated toward
/// zero. Rotated products must fit s32; their arithmetic shift rounds down.
///
/// `cornerAngle` uses 4096 units per turn. X points right and Y points up, so a
/// positive half-diagonal at zero angle points up and at a quarter turn points
/// right. The drawer subtracts Y from the screen centre. No pointer is retained.
static __inline__ s32 _dryfieldR08BeginDriftCorners(EffectBillboardScratch* projection, s16 sizeFactor, s32 cornerAngle)
{
    enum {
        DRYFIELD_R08_DRIFT_PERSPECTIVE_SCALE  = 47, // Multiplier in the depth-divided half-diagonal numerator
        DRYFIELD_R08_DRIFT_TRIG_FRACTION_BITS = 12  // Fractional bits in rsin/rcos samples; 4096 represents 1.0
    };
    s32 trigSample;
    s32 perspectiveNumerator;
    s32 halfDiagonalPixels;

    trigSample                = rsin(cornerAngle);
    perspectiveNumerator      = sizeFactor * DRYFIELD_R08_DRIFT_PERSPECTIVE_SCALE;
    halfDiagonalPixels        = perspectiveNumerator / projection->depth;
    projection->cornerOffsetX = (halfDiagonalPixels * trigSample) >> DRYFIELD_R08_DRIFT_TRIG_FRACTION_BITS;
    trigSample                = rcos(cornerAngle);
    halfDiagonalPixels        = perspectiveNumerator / projection->depth;
    projection->cornerOffsetY = (halfDiagonalPixels * trigSample) >> DRYFIELD_R08_DRIFT_TRIG_FRACTION_BITS;
    return perspectiveNumerator;
}

/// Writes a drift-sprite corner offset at an absolute screen-space angle.
///
/// `perspectiveNumerator` is the signed `sizeFactor * 47` returned by
/// `_dryfieldR08BeginDriftCorners`. The borrowed workspace requires positive
/// depth; division gives the pixel half-diagonal, truncated toward zero, before
/// Q12 rotation. Products must fit s32; their arithmetic shift rounds down.
///
/// Only the corner offsets change. `cornerAngle` uses 4096 units per turn with
/// X right and Y up; the drawer supplies the first angle plus a quarter turn
/// for the second pair of opposite corners. No pointer is retained.
static __inline__ void _dryfieldR08RotateDriftCorner(EffectBillboardScratch* projection, s32 perspectiveNumerator, s32 cornerAngle)
{
    enum { DRYFIELD_R08_DRIFT_TRIG_FRACTION_BITS = 12 }; // rsin/rcos fractional bits
    s32 halfDiagonalPixels;
    s32 trigSample;

    trigSample                = rsin(cornerAngle);
    halfDiagonalPixels        = perspectiveNumerator / projection->depth;
    projection->cornerOffsetX = (halfDiagonalPixels * trigSample) >> DRYFIELD_R08_DRIFT_TRIG_FRACTION_BITS;
    trigSample                = rcos(cornerAngle);
    halfDiagonalPixels        = perspectiveNumerator / projection->depth;
    projection->cornerOffsetY = (halfDiagonalPixels * trigSample) >> DRYFIELD_R08_DRIFT_TRIG_FRACTION_BITS;
}

/// Binds Dryfield R08's exported `void (Task*)` callback for this drift instance.
///
/// Function-identifier alias supplied before the fragment and cleared after it.
/// Used only as the definition name; no arguments, captures or constructed tokens.
#define EFFECT_SPRITE_DRIFT_TASK dryfieldR08SpriteDriftTask
/// Binds the twelve-frame drift drawer: `(const GfxCoord*, u16 frameAndPalette, s16 size, s16 angle)`.
///
/// Function-identifier alias supplied before the fragment and cleared after it.
/// Arguments are evaluated once; the coordinate is borrowed, size is a perspective
/// numerator, and angle uses 4096 units per turn. No captures or constructed tokens.
#define EFFECT_SPRITE_DRIFT_DRAW_BANKED _dryfieldR08DrawBankedDriftSprite
/// Binds the ten-frame drift drawer with the same coordinate, packed-frame, size and angle contract.
///
/// Function-identifier alias supplied before the fragment and cleared after it.
/// Arguments are evaluated once; the coordinate is borrowed, size is a perspective
/// numerator, and angle uses 4096 units per turn. No captures or constructed tokens.
#define EFFECT_SPRITE_DRIFT_DRAW_ALTERNATE _dryfieldR08DrawAlternateDriftSprite
/// Uses the banked drawer without the saved palette selector while sprite updates are suspended.
///
/// Define as 1 before including `effect_sprite_drift.inc.c`; 0 or undefined
/// retains the usual drawer and palette selection. Dryfield R08 enables this
/// override for every non-running effect-control value, including the final
/// redraw before cancellation, regardless of the task's drawer state. The cell
/// index, scale and rotation are retained; the banked drawer receives palette selector 0.
/// This takes precedence over `EFFECT_SPRITE_DRIFT_SIGN_BANK` for suspended draws
/// and is undefined at the end of the fragment.
#define EFFECT_SPRITE_DRIFT_PAUSED_DRAW_A_UNBANKED 1
#include "../../shared/effect_sprite_drift.inc.c"

/// Draws a rotating twelve-cell sprite with a per-cell or alternate palette.
///
/// `coord->workm` must be composed for `GsWSMATRIX`; only its translation,
/// narrowed to s16, is projected. `frameAndPalette` packs cell 0..11 in bits
/// 0..11 and a palette selector in bits 12..15: 0/1 use per-cell CLUT rows
/// 270/271, 2..15 use CLUT 0x428F. The five-column 48-texel sheet starts at
/// V=104 on texture page 0x2B; GPU byte UVs wrap across V=255.
///
/// `size * 47 / depth` is the signed pixel half-diagonal before Q12 rotation;
/// `angle` uses 4096 units per turn. Accepted projections require nonzero
/// SZ3/4 depth. A nonnegative GTE FLAG queues one additive raw-texture FT4,
/// requiring packet space at the primitive cursor. One initialized scratch
/// stack block is borrowed and released on every path; no pointer is retained.
/// A packet is reserved even when the projection is rejected.
static void _dryfieldR08DrawBankedDriftSprite(const GfxCoord* coord, u16 frameAndPalette, s16 size, s16 angle)
{
    enum {
        // Signed row offsets wrap to GPU UV bytes; last - first is 47 modulo 256.
        DRYFIELD_R08_DRIFT_FIRST_TEXEL_ROW     = 104,
        DRYFIELD_R08_DRIFT_LAST_TEXEL_ROW      = -105,
        DRYFIELD_R08_DRIFT_CLUT_ROW_SHIFT      = 6,
        DRYFIELD_R08_DRIFT_PALETTE_ROW_COUNT   = 2,
        DRYFIELD_R08_DRIFT_FRAME_MASK          = 0xFFF,
        DRYFIELD_R08_DRIFT_PALETTE_SHIFT       = 12,
        DRYFIELD_R08_DRIFT_CELLS_PER_ROW       = 5,
        DRYFIELD_R08_DRIFT_CELL_PITCH_TEXELS   = 48,
        DRYFIELD_R08_DRIFT_UV_SPAN_TEXELS      = 47,
        DRYFIELD_R08_DRIFT_QUARTER_TURN        = 0x400,
        DRYFIELD_R08_DRIFT_PACKET_WORDS        = sizeof(POLY_FT4) / sizeof(u32) - 1,
        DRYFIELD_R08_DRIFT_TEXTURED_QUAD_CODE  = 0x2C,
        DRYFIELD_R08_DRIFT_RAW_SEMITRANSPARENT = 3,
        DRYFIELD_R08_DRIFT_TEXTURE_PAGE        = 0x2B,
        DRYFIELD_R08_DRIFT_ALTERNATE_CLUT      = 0x428F,
        DRYFIELD_R08_DRIFT_FIRST_PALETTE_ROW   = 0x10E,
        DRYFIELD_R08_DRIFT_CLUT_COLUMN_MASK    = 0x3F
    };
    EffectBillboardScratch* projection;
    POLY_FT4*               quad;
    s32                     cornerAngle;
    s32                     perpendicularAngle;
    s32                     perspectiveNumerator;
    s32                     cellU;
    s32                     cellV;
    s32                     lastU;
    s32                     lastV;
    s32                     frameIndex;
    u16                     paletteSelector;

    projection                = SCRATCH_STACK_RESERVE_BLOCK(EffectBillboardScratch);
    projection->worldPoint.vx = coord->workm.t[0];
    projection->worldPoint.vy = coord->workm.t[1];
    projection->worldPoint.vz = coord->workm.t[2];
    frameIndex                = frameAndPalette & DRYFIELD_R08_DRIFT_FRAME_MASK;
    paletteSelector           = frameAndPalette >> DRYFIELD_R08_DRIFT_PALETTE_SHIFT;
    // Project the composed centre; the quad rotates only in screen space.
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&projection->worldPoint);
    gte_rtps();
    quad           = gGpuPrimCursor;
    gGpuPrimCursor = quad + 1;
    setlen(quad, DRYFIELD_R08_DRIFT_PACKET_WORDS);
    setcode(quad, DRYFIELD_R08_DRIFT_TEXTURED_QUAD_CODE);
    gte_stsxy(&projection->screenX);
    gte_stflg(&projection->projectionFlags);
    if (projection->projectionFlags >= 0) {
        gte_stszotz(&projection->depth);
        quad->tpage = DRYFIELD_R08_DRIFT_TEXTURE_PAGE;
        quad->code |= DRYFIELD_R08_DRIFT_RAW_SEMITRANSPARENT;
        if (paletteSelector >= DRYFIELD_R08_DRIFT_PALETTE_ROW_COUNT) {
            quad->clut = DRYFIELD_R08_DRIFT_ALTERNATE_CLUT;
        } else {
            quad->clut = ((paletteSelector + DRYFIELD_R08_DRIFT_FIRST_PALETTE_ROW) << DRYFIELD_R08_DRIFT_CLUT_ROW_SHIFT) | (frameIndex & DRYFIELD_R08_DRIFT_CLUT_COLUMN_MASK);
        }
        cornerAngle = angle;
        cellU       = ((u16)frameIndex % DRYFIELD_R08_DRIFT_CELLS_PER_ROW) * DRYFIELD_R08_DRIFT_CELL_PITCH_TEXELS;
        cellV       = ((u16)frameIndex / DRYFIELD_R08_DRIFT_CELLS_PER_ROW) * DRYFIELD_R08_DRIFT_CELL_PITCH_TEXELS;
        lastU       = cellU + DRYFIELD_R08_DRIFT_UV_SPAN_TEXELS;
        lastV       = cellV + DRYFIELD_R08_DRIFT_LAST_TEXEL_ROW;
        cellV       = cellV + DRYFIELD_R08_DRIFT_FIRST_TEXEL_ROW;
        setUV4(quad, cellU, cellV, lastU, cellV, cellU, lastV, lastU, lastV);
        perspectiveNumerator = _dryfieldR08BeginDriftCorners(projection, size, cornerAngle);
        quad->x0             = projection->screenX + (u16)projection->cornerOffsetX;
        quad->x3             = projection->screenX - (u16)projection->cornerOffsetX;
        quad->y0             = projection->screenY - (u16)projection->cornerOffsetY;
        quad->y3             = projection->screenY + (u16)projection->cornerOffsetY;
        perpendicularAngle   = cornerAngle + DRYFIELD_R08_DRIFT_QUARTER_TURN;
        _dryfieldR08RotateDriftCorner(projection, perspectiveNumerator, perpendicularAngle);
        quad->x1 = projection->screenX + (u16)projection->cornerOffsetX;
        quad->x2 = projection->screenX - (u16)projection->cornerOffsetX;
        quad->y1 = projection->screenY - (u16)projection->cornerOffsetY;
        quad->y2 = projection->screenY + (u16)projection->cornerOffsetY;
        // Sort by projection depth; the GPU consumes a complete textured-quad packet.
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)projection->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                quad);
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectBillboardScratch);
}

/// Draws a rotating ten-cell sprite with a base or alternate palette.
///
/// `coord->workm` must be composed for `GsWSMATRIX`; its translation is
/// narrowed to s16. Bits 0..11 of `frameAndPalette` hold cell 0..9; any bit
/// in 12..15 selects CLUT 0x428F instead of 0x43D0. The five-column sheet
/// has 48-texel cells starting at V=128 on texture page 0x2C.
///
/// `size * 47 / depth` is the signed pixel half-diagonal before Q12 rotation;
/// `angle` uses 4096 units per turn. Accepted projections require nonzero
/// SZ3/4 depth. A nonnegative GTE FLAG queues an additive raw-texture FT4.
/// The initialized scratch stack and primitive cursor must provide one block
/// and one packet; scratch is released on every path and no pointer is retained.
/// A packet is reserved even when the projection is rejected.
static void _dryfieldR08DrawAlternateDriftSprite(const GfxCoord* coord, u16 frameAndPalette, s16 size, s16 angle)
{
    enum {
        // Signed row offsets wrap to GPU UV bytes; last - first is 47 modulo 256.
        DRYFIELD_R08_DRIFT_FIRST_TEXEL_ROW     = -128,
        DRYFIELD_R08_DRIFT_LAST_TEXEL_ROW      = -81,
        DRYFIELD_R08_DRIFT_FRAME_MASK          = 0xFFF,
        DRYFIELD_R08_DRIFT_PALETTE_SHIFT       = 12,
        DRYFIELD_R08_DRIFT_CELLS_PER_ROW       = 5,
        DRYFIELD_R08_DRIFT_CELL_PITCH_TEXELS   = 48,
        DRYFIELD_R08_DRIFT_UV_SPAN_TEXELS      = 47,
        DRYFIELD_R08_DRIFT_QUARTER_TURN        = 0x400,
        DRYFIELD_R08_DRIFT_PACKET_WORDS        = sizeof(POLY_FT4) / sizeof(u32) - 1,
        DRYFIELD_R08_DRIFT_TEXTURED_QUAD_CODE  = 0x2C,
        DRYFIELD_R08_DRIFT_RAW_SEMITRANSPARENT = 3,
        DRYFIELD_R08_DRIFT_TEXTURE_PAGE        = 0x2C,
        DRYFIELD_R08_DRIFT_ALTERNATE_CLUT      = 0x428F,
        DRYFIELD_R08_DRIFT_BASE_CLUT           = 0x43D0
    };
    EffectBillboardScratch* projection;
    POLY_FT4*               quad;
    s32                     cornerAngle;
    s32                     perpendicularAngle;
    s32                     perspectiveNumerator;
    s32                     cellU;
    s32                     cellV;
    s32                     lastU;
    s32                     lastV;
    u16                     frameIndex;
    u16                     paletteSelector;

    projection                = SCRATCH_STACK_RESERVE_BLOCK(EffectBillboardScratch);
    projection->worldPoint.vx = coord->workm.t[0];
    paletteSelector           = frameAndPalette >> DRYFIELD_R08_DRIFT_PALETTE_SHIFT;
    projection->worldPoint.vy = coord->workm.t[1];
    projection->worldPoint.vz = coord->workm.t[2];
    frameIndex                = frameAndPalette & DRYFIELD_R08_DRIFT_FRAME_MASK;
    // Project the composed centre; the quad rotates only in screen space.
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&projection->worldPoint);
    gte_rtps();
    quad           = gGpuPrimCursor;
    gGpuPrimCursor = quad + 1;
    setlen(quad, DRYFIELD_R08_DRIFT_PACKET_WORDS);
    setcode(quad, DRYFIELD_R08_DRIFT_TEXTURED_QUAD_CODE);
    gte_stsxy(&projection->screenX);
    gte_stflg(&projection->projectionFlags);
    if (projection->projectionFlags >= 0) {
        gte_stszotz(&projection->depth);
        quad->tpage = DRYFIELD_R08_DRIFT_TEXTURE_PAGE;
        quad->code |= DRYFIELD_R08_DRIFT_RAW_SEMITRANSPARENT;
        if (paletteSelector) {
            quad->clut = DRYFIELD_R08_DRIFT_ALTERNATE_CLUT;
        } else {
            quad->clut = DRYFIELD_R08_DRIFT_BASE_CLUT;
        }
        cornerAngle = angle;
        cellU       = (frameIndex % DRYFIELD_R08_DRIFT_CELLS_PER_ROW) * DRYFIELD_R08_DRIFT_CELL_PITCH_TEXELS;
        cellV       = (frameIndex / DRYFIELD_R08_DRIFT_CELLS_PER_ROW) * DRYFIELD_R08_DRIFT_CELL_PITCH_TEXELS;
        lastU       = cellU + DRYFIELD_R08_DRIFT_UV_SPAN_TEXELS;
        lastV       = cellV + DRYFIELD_R08_DRIFT_LAST_TEXEL_ROW;
        cellV       = cellV + DRYFIELD_R08_DRIFT_FIRST_TEXEL_ROW;
        setUV4(quad, cellU, cellV, lastU, cellV, cellU, lastV, lastU, lastV);
        perspectiveNumerator = _dryfieldR08BeginDriftCorners(projection, size, cornerAngle);
        quad->x0             = projection->screenX + projection->cornerOffsetX;
        quad->x3             = projection->screenX - projection->cornerOffsetX;
        quad->y0             = projection->screenY - projection->cornerOffsetY;
        quad->y3             = projection->screenY + projection->cornerOffsetY;
        perpendicularAngle   = cornerAngle + DRYFIELD_R08_DRIFT_QUARTER_TURN;
        _dryfieldR08RotateDriftCorner(projection, perspectiveNumerator, perpendicularAngle);
        quad->x1 = projection->screenX + projection->cornerOffsetX;
        quad->x2 = projection->screenX - projection->cornerOffsetX;
        quad->y1 = projection->screenY - projection->cornerOffsetY;
        quad->y2 = projection->screenY + projection->cornerOffsetY;
        // Sort by projection depth; the GPU consumes a complete textured-quad packet.
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)projection->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                quad);
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectBillboardScratch);
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

    block = SCRATCH_STACK_RESERVE_BLOCK(RoomDiscScratch);

    gte_SetTransMatrix(&gGfxViewCoord.workm);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_ldv0(arg0);
    gte_rtps();
    gte_stsxy(&block->sx);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        arg1             <<= 16;
        arg1             >>= 16;
        outer              = (arg1 * 64) / block->otz;
        frame              = gDisplayState.animFrame;
        block->outerRadius = outer;
        inner              = (arg1 * 8) / block->otz;
        ang                = 0;
        packed             = arg2 << 16;
        blend              = (frame & 1) << (packed >> 28);
        r                  = blend + ((packed >> 20) & 0xF0);
        g                  = blend + ((packed >> 16) & 0xF0);
        b                  = blend + ((arg2 & 0xF) << 4);
        block->innerRadius = inner;
        do {
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, r, g, b);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->outerRadius * rsin(ang)) >> 12);
            t        = ang + 0x100;
            prim->y0 = block->sy + ((block->outerRadius * rcos(ang)) >> 12);
            prim->x1 = block->sx + ((block->outerRadius * rsin(t)) >> 12);
            prim->y1 = block->sy + ((block->outerRadius * rcos(t)) >> 12);
            t2       = ang + 0x200;
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->outerRadius * rsin(t2)) >> 12);
            prim->y3 = block->sy + ((block->outerRadius * rcos(t2)) >> 12);
            ang      = t2;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            gpuSetPrimitiveBlendMode(prim, GPU_BLEND_ADD, block->otz);
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
            prim->x0 = block->sx + ((block->innerRadius * rsin(ua)) >> 12);
            prim->y0 = block->sy + ((block->innerRadius * rcos(ua)) >> 12);
            prim->x1 = block->sx + ((block->outerRadius * rsin(ang)) >> 12);
            prim->y1 = block->sy + ((block->outerRadius * rcos(ang)) >> 12);
            ub       = ang + 0x400;
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->innerRadius * rsin(ub)) >> 12);
            prim->y3 = block->sy + ((block->innerRadius * rcos(ub)) >> 12);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            gpuSetPrimitiveBlendMode(prim, GPU_BLEND_ADD, block->otz);

            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, r, g, b);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->innerRadius * rsin(ang)) >> 12);
            prim->y0 = block->sy + ((block->innerRadius * rcos(ang)) >> 12);
            prim->x1 = block->sx + ((block->outerRadius * rsin(ub)) >> 11);
            prim->y1 = block->sy + ((block->outerRadius * rcos(ub)) >> 11);
            uc       = ang + 0x800;
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->innerRadius * rsin(uc)) >> 12);
            prim->y3 = block->sy + ((block->innerRadius * rcos(uc)) >> 12);
            ang      = uc;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            gpuSetPrimitiveBlendMode(prim, GPU_BLEND_ADD, block->otz);
        } while (ang < 0x1000);
    }
    SCRATCH_STACK_RELEASE_BLOCK(RoomDiscScratch);
}

void func_dryfield_r08_8017F334(s32 arg0)
{
    D_dryfield_r08_80180C24 = arg0;
}

/// Sets the skip-OT-link byte (`SpriteBatch.hidden`) of command record
/// `arg0` + 1 in this room's sprite-table command list: non-zero leaves that
/// record's prims out of the ordering table. `arg0` is a view index below
/// 0xB; the record the table yields is larger than its `SpriteView` prefix,
/// so `[3].field_4` reaches the command list its tail holds there.
void func_dryfield_r08_8017F340(u8 arg0, u8 arg1)
{
    GameLocationKey* sess;
    SpriteBatch*     batches;

    sess = &gGameSession->location.loc;
    if ((u32)(arg0 & 0xFF) < 0xBU) {
        batches = Gp_SprtTables[sess->stage - 1]->areaViews[sess->area - 1][3].batches;
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
    SpriteView*      rec;
    SpriteBatch*     batches;

    sess = &gGameSession->location.loc;
    if ((u32)(arg0 & 0xFF) < 3U) {
        rec = Gp_SprtTables[sess->stage - 1]->areaViews[sess->area - 1];
        if ((u32)(arg0 & 0xFF) == 0U) {
            batches = rec[1].batches;
        } else {
            batches = rec[2].batches;
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
        D_dryfield_r08_8017F708[0].lights = &D_dryfield_r08_801809C0;
        return;
    }
    D_dryfield_r08_8017F708[0].lights = &D_dryfield_r08_80180B58;
}
