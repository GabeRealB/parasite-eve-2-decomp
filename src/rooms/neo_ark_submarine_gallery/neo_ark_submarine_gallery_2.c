#include "rooms/neo_ark_submarine_gallery.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "common.h"
#include "gte.h"

#include "neo_ark_submarine_gallery_private.h"

#include "actors/task_tables.h"

#include "gameplay/display.h"
#include "gameplay/actor_render.h"
#include "gameplay/area.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/effects.h"
#include "gameplay/light.h"
#include "gameplay/loading.h"
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
#include "main/scratch.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "mapui/map_neo_ark.h"

#include "overlay.h"

#include "rooms/room.h"

#include "rooms/room_common.h"
/// Empty presence flag so `water_effects.h` declares the shared
/// `_waterDrawSpinU16` and `_waterDrawTileU16`. This file includes their bodies.
#define WATER_SHARED_U16_DRAWERS
#include "../../shared/water_effects.h"
#include "../../shared/glow_draw.h"

#define D_neo_ark_submarine_gallery_801818D8 (D_neo_ark_submarine_gallery_801818C8 + 2)
#define D_neo_ark_submarine_gallery_801818F8 (D_neo_ark_submarine_gallery_801818C8 + 6)
#define D_neo_ark_submarine_gallery_80181928 (D_neo_ark_submarine_gallery_801818C8 + 12)

enum { NEO_ARK_SUBMARINE_GALLERY_PRISM_FIRST_VERTEX = 32 };

static void _neoArkSubmarineGalleryDrawLightPrism(const GfxCoord* coord, s16 firstVertex);

// Indexed views below share one contiguous table.
extern TaskDesc D_actor_100400_80147E48;

extern WorldCollisionSurfaceProperties D_neo_ark_submarine_gallery_801858D4[1];
extern WorldCollisionSurfaceProperties D_neo_ark_submarine_gallery_801858DC[1];
extern WorldCollisionSurfaceProperties D_neo_ark_submarine_gallery_801858E4[1];

TaskDesc D_neo_ark_submarine_gallery_8018186C = { { { TASK_BODY_NONE, 192 } }, waterRefractionTask, { .value = 0 } };

TaskDesc D_neo_ark_submarine_gallery_80181878 = { { { TASK_BODY_NONE, 192 } }, waterDistortBandTask, { .value = 0 } };

TaskMessageEntry D_neo_ark_submarine_gallery_80181884[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, func_neo_ark_submarine_gallery_8017EA0C },
    { 5105, func_neo_ark_submarine_gallery_8017EA04 },
    { DIRECTION_MESSAGE_ROOM_ACTION, func_neo_ark_submarine_gallery_8017EB48 },
    { ROOM_MESSAGE_COMMAND, func_neo_ark_submarine_gallery_8017EABC },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc D_neo_ark_submarine_gallery_801818AC = { { { TASK_BODY_NONE, 32 } }, func_neo_ark_submarine_gallery_8017E86C, { .value = 0 } };

s16 D_neo_ark_submarine_gallery_801818B8 = 0;

TaskDesc D_neo_ark_submarine_gallery_801818BC[1] = {
    { { { TASK_BODY_COORD, 96 } }, func_neo_ark_submarine_gallery_8017EF94, { .value = 0 } },
};

SVECTOR D_neo_ark_submarine_gallery_801818C8[40] = {
    { -760, 2800, 3820, 0 },
    { -760, 3200, 3820, 0 },
    { 2170, 2800, 3250, 0 },
    { 2170, 3200, 3250, 0 },
    { 3830, 2800, 760, 0 },
    { 3830, 3200, 760, 0 },
    { 3250, 2800, -2170, 0 },
    { 3250, 3200, -2170, 0 },
    { 770, 2800, -3830, 0 },
    { 770, 3200, -3830, 0 },
    { -2170, 2800, -3250, 0 },
    { -2170, 3200, -3250, 0 },
    { -3830, 2800, -760, 0 },
    { -3830, 3200, -760, 0 },
    { -3250, 2800, 2170, 0 },
    { -3250, 3200, 2170, 0 },
    { -640, 4800, 3240, 0 },
    { -500, 4800, 2500, 0 },
    { 1840, 4800, 2740, 0 },
    { 1420, 4800, 2130, 0 },
    { 3240, 4800, 640, 0 },
    { 2500, 4800, 500, 0 },
    { 2750, 4800, -1830, 0 },
    { 2120, 4800, -1420, 0 },
    { 640, 4800, -3240, 0 },
    { 500, 4800, -2500, 0 },
    { -1830, 4800, -2750, 0 },
    { -1420, 4800, -2120, 0 },
    { -3240, 4800, -640, 0 },
    { -2500, 4800, -500, 0 },
    { -2740, 4800, 1830, 0 },
    { -2120, 4800, 1420, 0 },
    { 820, 2110, 3780, 0 },
    { 1610, 2110, 3500, 0 },
    { 1340, 2110, 2710, 0 },
    { 540, 2110, 2990, 0 },
    { 820, 3400, 3780, 0 },
    { 1610, 3400, 3500, 0 },
    { 1340, 3400, 2710, 0 },
    { 540, 3400, 2990, 0 },
};

u8* D_neo_ark_submarine_gallery_80181A08[1] = {
    gViewIdentityMap,
};

ViewCount D_neo_ark_submarine_gallery_80181A0C[1] = { 7 };

DirectionWarpEntry D_neo_ark_submarine_gallery_80181A10[1] = {
    { { { .word = 2304 }, 1300, 5000, 3100 }, { 0, 0, 0, 0 }, { { .word = 2304 }, 1300, 5000, 3100 }, { 0, 0, 0, 0 }, 0x551E0001, 0x551E0001, DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_FADE_DEPARTURE, DIRECTION_WARP_MAP_FLAG_NONE },
};

// Height override read by the shared waypoint actor.
s16 D_neo_ark_submarine_gallery_80181A48 = 5000;

// Retained data: Eight-point coordinate pool explicitly addressed by the following pointer table; no runtime owner found.
SVECTOR D_neo_ark_submarine_gallery_80181A4C[8] = {
    { 1000, 1500, 0, 0 },
    { -1000, 1500, 0, 0 },
    { 0, 1500, 1000, 0 },
    { 0, 1500, -1000, 0 },
    { 1000, 1500, 0, 0 },
    { -1000, 1500, 0, 0 },
    { 0, 1500, 1000, 0 },
    { 0, 1500, -1000, 0 },
};

// Retained data: Eight-point coordinate pool explicitly addressed by the following pointer table; no runtime owner found.
SVECTOR D_neo_ark_submarine_gallery_80181A8C[8] = {
    { 0, 1500, -1000, 0 },
    { 1000, 1500, 0, 0 },
    { -1000, 1500, 0, 0 },
    { 0, 1500, 1000, 0 },
    { 0, 1500, -1000, 0 },
    { 1000, 1500, 0, 0 },
    { -1000, 1500, 0, 0 },
    { 0, 1500, 1000, 0 },
};

// Retained data: Eight-point coordinate pool explicitly addressed by the following pointer table; no runtime owner found.
SVECTOR D_neo_ark_submarine_gallery_80181ACC[8] = {
    { 0, 1500, 1000, 0 },
    { 0, 1500, -1000, 0 },
    { 1000, 1500, 0, 0 },
    { -1000, 1500, 0, 0 },
    { 0, 1500, 1000, 0 },
    { 0, 1500, -1000, 0 },
    { 1000, 1500, 0, 0 },
    { -1000, 1500, 0, 0 },
};

// Retained data: Four retained references to the preceding eight-point coordinate pools; no runtime owner found.
SVECTOR* D_neo_ark_submarine_gallery_80181B0C[4] = {
    D_neo_ark_submarine_gallery_80181A4C,
    D_neo_ark_submarine_gallery_80181A8C,
    D_neo_ark_submarine_gallery_80181ACC,
    D_neo_ark_submarine_gallery_80181A4C,
};

// Retained data: Adjacent retained point data, ending with fourth halfword -1; no runtime owner found.
SVECTOR D_neo_ark_submarine_gallery_80181B1C[6] = {
    { 0, 0, 0, 0 },
    { 1000, 1500, 0, 0 },
    { -1000, 1500, 0, 0 },
    { 0, 1500, 1000, 0 },
    { 0, 1500, -1000, 0 },
    { 0, 0, 0, -1 },
};

static SVECTOR _gNeoArkSubmarineGalleryCollision04DDCNormals[31] = {
#include "assets/neo_ark_submarine_gallery_collision_04DDC_normals.inc"
};

static SVECTOR _gNeoArkSubmarineGalleryCollision04DDCVerts[104] = {
#include "assets/neo_ark_submarine_gallery_collision_04DDC_verts.inc"
};

static WorldCollisionGridFace _gNeoArkSubmarineGalleryCollision04DDCFaces[50] = {
#include "assets/neo_ark_submarine_gallery_collision_04DDC_faces.inc"
};

static s16 _gNeoArkSubmarineGalleryCollision04DDCCells[206] = {
#include "assets/neo_ark_submarine_gallery_collision_04DDC_cells.inc"
};

#define GRID_CELL(i) (&_gNeoArkSubmarineGalleryCollision04DDCCells[i])
static s16* _gNeoArkSubmarineGalleryCollision04DDCTable[9] = {
#include "assets/neo_ark_submarine_gallery_collision_04DDC_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid D_neo_ark_submarine_gallery_8018239C = { NULL, _gNeoArkSubmarineGalleryCollision04DDCNormals, _gNeoArkSubmarineGalleryCollision04DDCVerts, _gNeoArkSubmarineGalleryCollision04DDCFaces, _gNeoArkSubmarineGalleryCollision04DDCTable, 5000, 5000, 3, 3, 4000, 50 };

ViewCamera D_neo_ark_submarine_gallery_801823C0[7] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { 0, 0x4E20, 0 } }, 603 },
    { { { { 2429, 0, -3297 }, { 0, 4096, 0 }, { 3297, 0, 2429 } }, { 3800, -4000, -200 } }, 257 },
    { { { { 0, 0, -4095 }, { -3996, 899, 0 }, { 899, 3996, 0 } }, { 3340, 27, 0 } }, 257 },
    { { { { -2429, 0, -3297 }, { 0, 4096, 0 }, { 3297, 0, -2429 } }, { 3800, -4000, 200 } }, 257 },
    { { { { 0, 0, -4096 }, { 0, 4096, 0 }, { 4096, 0, 0 } }, { 3000, -4140, 0 } }, 257 },
    { { { { -4096, 0, 0 }, { 0, 3973, -995 }, { 0, -995, -3973 } }, { -5, -4380, -3925 } }, 272 },
    { { { { -639, 0, 4045 }, { 1045, 3956, 165 }, { -3908, 1058, -617 } }, { -7620, -3215, -3705 } }, 272 },
};

SpriteBatch D_neo_ark_submarine_gallery_801824BC[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_neo_ark_submarine_gallery_801824CC[149] = {
    { 142, 0x3FC0, { .fields = { 8, 48 } }, 32, 40, 712, { .fields = { 48, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 64 } }, 40, 40, 637, { .fields = { 104, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, 48, 40, 587, { .fields = { 24, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 64 } }, 56, 56, 500, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 64 } }, 64, 56, 500, { .fields = { 112, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 64 } }, 72, 56, 375, { .fields = { 112, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 56 } }, 80, 64, 250, { .fields = { 56, 112 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 56 } }, 96, 64, 250, { .fields = { 64, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 56 } }, 112, 64, 250, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, 128, 72, 250, { .fields = { 40, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, 144, 72, 250, { .fields = { 40, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 24 } }, 56, 32, 1000, { .fields = { 8, 120 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 8, 32 } }, 64, 24, 1100, { .fields = { 112, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 72, 24, 1000, { .fields = { 32, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 80, 24, 1000, { .fields = { 8, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, 96, 24, 1000, { .fields = { 16, 216 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 32 } }, 112, 24, 1000, { .fields = { 104, 32 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 32 } }, 128, 24, 1000, { .fields = { 104, 224 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 32 } }, 144, 24, 1000, { .fields = { 104, 192 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 16 } }, -152, -120, 700, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 16 } }, -136, -120, 875, { .fields = { 80, 80 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 32 } }, -120, -120, 950, { .fields = { 104, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -104, -120, 1000, { .fields = { 8, 184 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 24 } }, 128, -72, 1875, { .fields = { 88, 208 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 24 } }, 112, -72, 1875, { .fields = { 88, 232 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 24 } }, 96, -72, 1875, { .fields = { 96, 112 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 24 } }, 80, -72, 1875, { .fields = { 96, 88 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 24 } }, 64, -72, 1875, { .fields = { 96, 64 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 24 } }, 48, -72, 1875, { .fields = { 96, 136 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 16 } }, 48, 16, 1875, { .fields = { 72, 48 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 16 } }, 64, 16, 1875, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 80, 16, 1875, { .fields = { 40, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 96, 16, 1875, { .fields = { 48, 240 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 16 } }, 112, 16, 1875, { .fields = { 80, 16 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 16 } }, 128, 16, 1875, { .fields = { 80, 32 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 16 } }, 144, 16, 1875, { .fields = { 88, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 64 } }, 152, -48, 1875, { .fields = { 88, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 64 } }, -160, 40, 625, { .fields = { 112, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 56 } }, -152, 40, 750, { .fields = { 64, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 48 } }, -144, 40, 875, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -136, 32, 1000, { .fields = { 16, 96 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 40 } }, -120, 32, 1062, { .fields = { 120, 200 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 40 } }, -104, 24, 1125, { .fields = { 120, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -56, -24, 1300, { .fields = { 32, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, -40, -24, 1575, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 64 } }, -24, -24, 1687, { .fields = { 120, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 64 } }, -8, -24, 1750, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 64 } }, 8, -24, 1875, { .fields = { 120, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 64 } }, 24, -24, 1875, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 56 } }, 40, -24, 1875, { .fields = { 56, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, -88, -104, 1250, { .fields = { 112, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 64 } }, -56, -88, 1300, { .fields = { 120, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, -128, -40, 937, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -128, -104, 937, { .fields = { 8, 136 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 64 } }, -40, -88, 1575, { .fields = { 80, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 64 } }, -24, -88, 1687, { .fields = { 96, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -8, -72, 1750, { .fields = { 24, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, 8, -72, 1875, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, 24, -72, 1875, { .fields = { 24, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 48 } }, 40, -72, 1875, { .fields = { 24, 48 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 72, 8 } }, -64, 112, 725, { .fields = { 16, 224 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 64, 8 } }, 8, 112, 625, { .fields = { 24, 216 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 56, 8 } }, 0, 104, 725, { .fields = { 32, 232 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 64, 8 } }, -64, 104, 750, { .fields = { 24, 248 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 56, 8 } }, -56, 96, 830, { .fields = { 32, 240 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 56, 8 } }, 0, 96, 787, { .fields = { 32, 184 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 8 } }, -8, 88, 878, { .fields = { 48, 176 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 8 } }, -56, 88, 862, { .fields = { 40, 192 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 8 } }, -48, 80, 921, { .fields = { 48, 208 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 8 } }, -8, 80, 1017, { .fields = { 40, 200 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 32, 8 } }, -40, 72, 1012, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 8 } }, -8, 72, 1012, { .fields = { 32, 40 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 32, 8 } }, -40, 64, 1224, { .fields = { 48, 32 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 8 } }, -8, 64, 1224, { .fields = { 40, 72 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 32, 8 } }, -32, 56, 1176, { .fields = { 48, 64 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 32, 8 } }, 0, 56, 1352, { .fields = { 48, 16 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 32, 8 } }, 0, 48, 1500, { .fields = { 48, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, -64, -24, 1250, { .fields = { 40, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 72 } }, -72, -24, 1312, { .fields = { 32, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 64 } }, -64, -88, 1250, { .fields = { 96, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 80 } }, -72, -104, 1312, { .fields = { 104, 176 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 8, 24 } }, 152, -72, 1875, { .fields = { 96, 24 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 8, 24 } }, 144, -72, 1875, { .fields = { 96, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -88, -24, 1250, { .fields = { 32, 96 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 32 } }, -88, 24, 1250, { .fields = { 112, 64 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 16 } }, -88, -120, 1000, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 16 } }, -72, -120, 1000, { .fields = { 80, 176 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 8, 32 } }, -64, -120, 1000, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 80 } }, -160, -120, 3750, { .fields = { 64, 88 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 80 } }, -144, -120, 3750, { .fields = { 80, 176 } }, 128, 128, 128, 0 },
    { 141, 0x4000, { .fields = { 16, 16 } }, -128, -120, 3750, { .fields = { 80, 128 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 16, 56 } }, -128, -96, 3750, { .fields = { 72, 64 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 80 } }, -112, -120, 3750, { .fields = { 64, 168 } }, 128, 128, 128, 0 },
    { 141, 0x4000, { .fields = { 16, 24 } }, -96, -120, 3750, { .fields = { 88, 184 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 16, 48 } }, -96, -88, 3750, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 16, 40 } }, -80, -80, 3750, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 141, 0x4000, { .fields = { 16, 8 } }, -64, -48, 3750, { .fields = { 64, 24 } }, 128, 128, 128, 0 },
    { 141, 0x4000, { .fields = { 16, 16 } }, -48, -56, 3750, { .fields = { 80, 112 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 8 } }, -32, -48, 3750, { .fields = { 64, 248 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 16, 16 } }, -16, -56, 3750, { .fields = { 24, 240 } }, 128, 128, 128, 0 },
    { 141, 0x4000, { .fields = { 16, 16 } }, 0, -56, 3750, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 16, 24 } }, 16, -64, 3750, { .fields = { 64, 232 } }, 128, 128, 128, 0 },
    { 141, 0x4000, { .fields = { 16, 24 } }, 32, -64, 3750, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 141, 0x4000, { .fields = { 16, 16 } }, 48, -56, 3750, { .fields = { 80, 64 } }, 128, 128, 128, 0 },
    { 141, 0x4000, { .fields = { 16, 16 } }, 64, -56, 3750, { .fields = { 80, 160 } }, 128, 128, 128, 0 },
    { 141, 0x4000, { .fields = { 16, 32 } }, 80, -72, 3750, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 16, 40 } }, 96, -80, 3750, { .fields = { 0, 80 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 16, 40 } }, 112, -80, 3750, { .fields = { 0, 40 } }, 128, 128, 128, 0 },
    { 141, 0x4000, { .fields = { 16, 40 } }, 128, -80, 3750, { .fields = { 120, 120 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 16, 48 } }, 144, -88, 3750, { .fields = { 32, 48 } }, 128, 128, 128, 0 },
    { 143, 0x4040, { .fields = { 8, 88 } }, -160, -120, 750, { .fields = { 104, 88 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4040, { .fields = { 8, 88 } }, -160, -32, 750, { .fields = { 96, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4040, { .fields = { 8, 80 } }, -152, -32, 812, { .fields = { 96, 176 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4040, { .fields = { 8, 88 } }, -152, -120, 812, { .fields = { 112, 88 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4040, { .fields = { 8, 88 } }, -144, -120, 875, { .fields = { 120, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4040, { .fields = { 8, 80 } }, -144, -32, 875, { .fields = { 64, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4040, { .fields = { 8, 80 } }, -136, -32, 1000, { .fields = { 56, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4040, { .fields = { 8, 72 } }, -128, -32, 1062, { .fields = { 24, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4040, { .fields = { 8, 72 } }, -120, -32, 1125, { .fields = { 16, 144 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4040, { .fields = { 8, 88 } }, 40, -64, 1900, { .fields = { 120, 88 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4040, { .fields = { 16, 88 } }, 48, -64, 1875, { .fields = { 104, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4040, { .fields = { 16, 80 } }, 64, -56, 1875, { .fields = { 48, 80 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4040, { .fields = { 16, 80 } }, 80, -56, 1875, { .fields = { 48, 160 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4040, { .fields = { 16, 80 } }, 96, -56, 1875, { .fields = { 40, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4040, { .fields = { 16, 88 } }, 112, -64, 1875, { .fields = { 80, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4040, { .fields = { 16, 88 } }, 128, -64, 1875, { .fields = { 80, 88 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4040, { .fields = { 8, 64 } }, -80, -96, 1250, { .fields = { 104, 128 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4040, { .fields = { 8, 56 } }, -72, -88, 1325, { .fields = { 8, 200 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4040, { .fields = { 8, 64 } }, -72, -32, 1325, { .fields = { 96, 128 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4040, { .fields = { 8, 64 } }, -80, -32, 1250, { .fields = { 88, 128 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4040, { .fields = { 8, 64 } }, -88, -32, 1225, { .fields = { 80, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4040, { .fields = { 8, 64 } }, -88, -96, 1225, { .fields = { 88, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4040, { .fields = { 8, 32 } }, -112, -112, 750, { .fields = { 40, 224 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4040, { .fields = { 8, 32 } }, -104, -112, 750, { .fields = { 16, 224 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4040, { .fields = { 8, 72 } }, -104, -32, 1125, { .fields = { 24, 144 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4040, { .fields = { 8, 64 } }, -96, -32, 1125, { .fields = { 8, 72 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4040, { .fields = { 8, 48 } }, -96, -80, 1125, { .fields = { 56, 168 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4040, { .fields = { 8, 48 } }, -104, -80, 1125, { .fields = { 48, 96 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 141, 0x4040, { .fields = { 8, 24 } }, -96, -104, 750, { .fields = { 120, 96 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4040, { .fields = { 8, 48 } }, -112, -80, 1125, { .fields = { 16, 48 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4040, { .fields = { 8, 72 } }, -112, -32, 1125, { .fields = { 16, 72 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4040, { .fields = { 8, 32 } }, -120, -120, 750, { .fields = { 8, 224 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4040, { .fields = { 8, 56 } }, -120, -88, 1125, { .fields = { 80, 120 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 141, 0x4040, { .fields = { 8, 32 } }, -128, -120, 750, { .fields = { 120, 32 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4040, { .fields = { 8, 56 } }, -128, -88, 1062, { .fields = { 72, 120 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4040, { .fields = { 8, 32 } }, -136, -120, 750, { .fields = { 56, 216 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4040, { .fields = { 8, 56 } }, -136, -88, 1000, { .fields = { 72, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4040, { .fields = { 8, 88 } }, 144, -64, 1900, { .fields = { 72, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4040, { .fields = { 8, 88 } }, 152, -64, 1900, { .fields = { 96, 88 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
};

SpriteBatch D_neo_ark_submarine_gallery_80183070[6] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 19, 0, 0, { 3, 0 } },
    { 19, 69, 0, 0, { 0, 0 } },
    { 88, 22, 0, 0, { 2, 0 } },
    { 110, 39, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_neo_ark_submarine_gallery_801830A0[78] = {
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -104, -120, 1175, { .fields = { 80, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, -104, -104, 1175, { .fields = { 88, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -104, -88, 1175, { .fields = { 80, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, -96, -72, 1175, { .fields = { 80, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -88, -56, 1175, { .fields = { 96, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -72, -56, 1175, { .fields = { 112, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -56, -48, 1175, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -40, -40, 1175, { .fields = { 112, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -24, -32, 1175, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, -8, -32, 1175, { .fields = { 112, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 8, -32, 1175, { .fields = { 112, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 24, -40, 1175, { .fields = { 112, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 40, -48, 1175, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 72, -120, 1175, { .fields = { 80, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 16 } }, 80, -104, 1175, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 72, -88, 1175, { .fields = { 80, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 64, -72, 1175, { .fields = { 80, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, 56, -56, 1175, { .fields = { 112, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 72, -56, 1175, { .fields = { 96, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 8 } }, -128, 0, 1282, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 8 } }, -144, -120, 1425, { .fields = { 96, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 8 } }, -144, -112, 1425, { .fields = { 104, 248 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 8 } }, -144, -104, 1425, { .fields = { 104, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 8 } }, -144, -96, 1425, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 72, 8 } }, -144, -88, 1425, { .fields = { 80, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 72, 8 } }, -144, -80, 1425, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 72, 8 } }, -144, -72, 1425, { .fields = { 80, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 80, 8 } }, -144, -64, 1423, { .fields = { 0, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, -144, -56, 1398, { .fields = { 40, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -104, -56, 1398, { .fields = { 32, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, -144, -48, 1398, { .fields = { 0, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, -96, -48, 1398, { .fields = { 120, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 8 } }, -144, -40, 1389, { .fields = { 112, 152 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 8 } }, -88, -40, 1389, { .fields = { 120, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 8 } }, -144, -32, 1389, { .fields = { 88, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 80, 8 } }, -80, -32, 1389, { .fields = { 64, 224 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 80, 8 } }, 0, -32, 1325, { .fields = { 64, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 8 } }, 80, -32, 1325, { .fields = { 80, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 8 } }, 32, -40, 1350, { .fields = { 80, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 56, 8 } }, 88, -40, 1350, { .fields = { 72, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, 48, -48, 1350, { .fields = { 80, 32 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, 96, -48, 1350, { .fields = { 80, 16 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 48, 8 } }, 56, -56, 1350, { .fields = { 96, 8 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 40, 8 } }, 104, -56, 1350, { .fields = { 112, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 80, 8 } }, 64, -64, 1375, { .fields = { 72, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 72, 8 } }, 72, -72, 1375, { .fields = { 72, 56 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 72, 8 } }, 72, -80, 1400, { .fields = { 72, 104 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 72, 8 } }, 72, -88, 1400, { .fields = { 72, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 64, 8 } }, 80, -96, 1425, { .fields = { 80, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 8 } }, 80, -104, 1425, { .fields = { 40, 248 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 8 } }, 80, -112, 1425, { .fields = { 40, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 8 } }, 80, -120, 1425, { .fields = { 48, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 72, 8 } }, -128, -24, 1360, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 104, 8 } }, -56, -24, 1360, { .fields = { 120, 24 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 96, 8 } }, 48, -24, 1405, { .fields = { 0, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 80, 8 } }, -128, -16, 1360, { .fields = { 16, 8 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 96, 8 } }, -48, -16, 1312, { .fields = { 16, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 96, 8 } }, 48, -16, 1392, { .fields = { 16, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 88, 8 } }, -40, -8, 1348, { .fields = { 24, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 96, 8 } }, 48, -8, 1387, { .fields = { 16, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 88, 8 } }, -128, -8, 1348, { .fields = { 24, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 88, 8 } }, -120, 0, 1364, { .fields = { 24, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 88, 8 } }, -32, 0, 1348, { .fields = { 24, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 72, 8 } }, 56, 0, 1387, { .fields = { 40, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 88, 8 } }, -112, 8, 1364, { .fields = { 120, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 88, 8 } }, -24, 8, 1323, { .fields = { 120, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 8 } }, 64, 8, 1323, { .fields = { 16, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 88, 8 } }, -32, 16, 1371, { .fields = { 120, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 72, 8 } }, 56, 16, 1371, { .fields = { 8, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 80, 8 } }, -96, 24, 1342, { .fields = { 0, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 8 } }, -16, 24, 1342, { .fields = { 16, 104 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 64, 8 } }, 48, 24, 1342, { .fields = { 16, 56 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 72, 8 } }, -96, 32, 1298, { .fields = { 24, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 72, 8 } }, -24, 32, 1298, { .fields = { 24, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 8 } }, 48, 32, 1298, { .fields = { 48, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 72, 8 } }, -80, 40, 1298, { .fields = { 16, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 88, 8 } }, -8, 40, 1298, { .fields = { 120, 48 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 80, 8 } }, -112, 16, 1364, { .fields = { 0, 40 } }, 128, 128, 128, 0 },
};

SpriteBatch D_neo_ark_submarine_gallery_801836B8[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 19, 0, 0, { 1, 0 } },
    { 19, 59, 0, 0, { 2, 0 } },
    { 78, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_neo_ark_submarine_gallery_801836E0[101] = {
    { 141, 0x3FC0, { .fields = { 16, 32 } }, -160, 88, 250, { .fields = { 112, 64 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 32 } }, -144, 88, 250, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -128, 88, 250, { .fields = { 0, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -112, 72, 250, { .fields = { 112, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 64 } }, -96, 56, 250, { .fields = { 80, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 64 } }, -80, 56, 450, { .fields = { 80, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -160, 24, 1125, { .fields = { 0, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -144, 24, 1156, { .fields = { 32, 168 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -128, 24, 1125, { .fields = { 32, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -112, 24, 1125, { .fields = { 0, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -96, 24, 1125, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 24 } }, -80, 32, 1125, { .fields = { 48, 232 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -64, 72, 500, { .fields = { 16, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 32 } }, -32, 48, 750, { .fields = { 8, 32 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 24 } }, -64, 32, 1000, { .fields = { 112, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -64, 56, 375, { .fields = { 16, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 56 } }, -40, 48, 750, { .fields = { 56, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 40 } }, -48, 32, 849, { .fields = { 8, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, -48, 72, 695, { .fields = { 0, 216 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -160, -88, 1875, { .fields = { 32, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -144, -88, 1875, { .fields = { 32, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -128, -88, 1875, { .fields = { 32, 88 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -112, -88, 1875, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -96, -88, 1875, { .fields = { 16, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -80, -88, 1875, { .fields = { 16, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -64, -88, 1875, { .fields = { 16, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -48, -88, 1875, { .fields = { 16, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -32, -88, 1875, { .fields = { 0, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -16, -88, 1875, { .fields = { 0, 96 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 32 } }, 0, -88, 1875, { .fields = { 120, 32 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 16 } }, -160, 16, 2000, { .fields = { 104, 48 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 16 } }, -144, 16, 2000, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 16 } }, -128, 16, 2000, { .fields = { 96, 64 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 16 } }, -112, 16, 2000, { .fields = { 104, 32 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 16 } }, -96, 16, 2000, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -80, 16, 1875, { .fields = { 16, 240 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 16 } }, -64, 16, 1875, { .fields = { 96, 16 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 24 } }, -48, 16, 1875, { .fields = { 112, 192 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 24 } }, -32, 16, 1875, { .fields = { 112, 168 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 24 } }, -16, 16, 1875, { .fields = { 112, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, -48, -48, 1875, { .fields = { 104, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 64 } }, -104, -48, 1700, { .fields = { 64, 0 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 64, 8 } }, -80, 112, 625, { .fields = { 48, 168 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 8 } }, -16, 112, 625, { .fields = { 64, 160 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 8 } }, -64, 104, 625, { .fields = { 64, 128 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 8 } }, -16, 104, 625, { .fields = { 64, 176 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 8 } }, -64, 96, 771, { .fields = { 64, 152 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 48, 8 } }, -16, 96, 771, { .fields = { 64, 144 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 8 } }, -48, 88, 1006, { .fields = { 72, 136 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 8 } }, -8, 88, 1006, { .fields = { 72, 96 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 8 } }, -48, 80, 1120, { .fields = { 72, 112 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 40, 8 } }, -8, 80, 1120, { .fields = { 72, 104 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 64, 8 } }, -32, 72, 1190, { .fields = { 48, 120 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 64, 8 } }, -32, 64, 1288, { .fields = { 48, 88 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 64, 8 } }, -32, 56, 1324, { .fields = { 48, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, 0, -56, 1875, { .fields = { 0, 0 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 24 } }, 0, 16, 1875, { .fields = { 112, 144 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 24 } }, 16, 16, 1875, { .fields = { 112, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, 16, -88, 1875, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 56 } }, 16, -40, 1875, { .fields = { 48, 120 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 64 } }, 32, -104, 1687, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 64 } }, 32, -40, 1687, { .fields = { 64, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 64 } }, 48, -104, 1625, { .fields = { 96, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 64 } }, 48, -40, 1625, { .fields = { 112, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, 64, -104, 1625, { .fields = { 112, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 64 } }, 64, -40, 1625, { .fields = { 96, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 64 } }, 80, -40, 1500, { .fields = { 96, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 64 } }, 80, -104, 1500, { .fields = { 48, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 64 } }, 96, -104, 1375, { .fields = { 64, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 64 } }, 96, -40, 1375, { .fields = { 64, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 64 } }, 112, -32, 1125, { .fields = { 80, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, 112, -104, 1125, { .fields = { 0, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 72 } }, 128, -104, 875, { .fields = { 112, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 72 } }, 128, -32, 875, { .fields = { 0, 72 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, 144, -104, 875, { .fields = { 56, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, 144, -24, 875, { .fields = { 16, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 72 } }, -160, -64, 3750, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 64 } }, -144, -56, 3750, { .fields = { 80, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 56 } }, -128, -48, 3750, { .fields = { 48, 64 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -112, -24, 3750, { .fields = { 0, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 80 } }, -96, -72, 3750, { .fields = { 72, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 88 } }, -80, -80, 3750, { .fields = { 88, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 96 } }, -64, -88, 3750, { .fields = { 104, 96 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 88 } }, -160, -64, 1900, { .fields = { 32, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 16, 88 } }, -144, -64, 1900, { .fields = { 64, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 16, 88 } }, -128, -64, 1900, { .fields = { 88, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 16, 80 } }, -112, -56, 1900, { .fields = { 16, 80 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 16, 80 } }, -96, -56, 1900, { .fields = { 40, 176 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 16, 80 } }, -80, -56, 1900, { .fields = { 88, 176 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 16, 88 } }, -64, -64, 1900, { .fields = { 64, 88 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 16, 88 } }, -48, -64, 1900, { .fields = { 32, 88 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 16, 88 } }, -32, -64, 1900, { .fields = { 48, 88 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 16, 88 } }, -16, -64, 1900, { .fields = { 48, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 96 } }, 0, -72, 1900, { .fields = { 120, 96 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 96 } }, 8, -72, 1900, { .fields = { 104, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 16, 80 } }, 144, -120, 862, { .fields = { 16, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 16, 96 } }, 144, -40, 862, { .fields = { 112, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 88 } }, 128, -40, 900, { .fields = { 80, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 88 } }, 136, -40, 862, { .fields = { 80, 88 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 80 } }, 136, -120, 862, { .fields = { 32, 176 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 142, 0x4000, { .fields = { 8, 56 } }, 128, -96, 900, { .fields = { 48, 176 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
};

SpriteBatch D_neo_ark_submarine_gallery_80183EC4[6] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 19, 0, 0, { 3, 0 } },
    { 19, 57, 0, 0, { 0, 0 } },
    { 76, 7, 0, 0, { 2, 0 } },
    { 83, 18, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_neo_ark_submarine_gallery_80183EF4[123] = {
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -160, 24, 625, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -144, 24, 750, { .fields = { 64, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -128, 24, 750, { .fields = { 32, 184 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -80, 16, 936, { .fields = { 112, 216 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 32 } }, -64, 16, 1095, { .fields = { 80, 64 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 32 } }, -48, 16, 1095, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 32 } }, 0, 16, 1095, { .fields = { 80, 192 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 32 } }, 16, 16, 1095, { .fields = { 96, 112 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 32 } }, 32, 16, 1015, { .fields = { 96, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 80, 16, 856, { .fields = { 16, 120 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 32 } }, 96, 24, 825, { .fields = { 80, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 112, 24, 750, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 32 } }, 48, 16, 968, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 64, 16, 904, { .fields = { 0, 40 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 32 } }, -112, 24, 763, { .fields = { 80, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -96, 16, 923, { .fields = { 16, 200 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 24 } }, -32, 24, 1096, { .fields = { 64, 168 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 24 } }, -16, 24, 1096, { .fields = { 64, 24 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 128, 32, 627, { .fields = { 16, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, 144, 32, 600, { .fields = { 80, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -160, 80, 356, { .fields = { 24, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -144, 80, 250, { .fields = { 16, 40 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 40 } }, -128, 80, 250, { .fields = { 112, 160 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -112, 80, 250, { .fields = { 0, 80 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -96, 80, 250, { .fields = { 32, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -80, 80, 250, { .fields = { 16, 80 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 32 } }, -64, 88, 250, { .fields = { 96, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 32 } }, -48, 88, 250, { .fields = { 32, 224 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 16 } }, -32, 104, 250, { .fields = { 96, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 8 } }, -16, 112, 250, { .fields = { 96, 248 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 8 } }, 0, 112, 250, { .fields = { 48, 216 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 8 } }, 16, 112, 250, { .fields = { 48, 208 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 16 } }, 32, 104, 250, { .fields = { 48, 144 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 16 } }, 48, 104, 250, { .fields = { 48, 128 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 64, 104, 250, { .fields = { 64, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 80, 104, 250, { .fields = { 16, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 96, 104, 250, { .fields = { 48, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, 112, 104, 250, { .fields = { 80, 240 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 24 } }, 128, 96, 250, { .fields = { 64, 72 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, 144, 72, 250, { .fields = { 80, 96 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 24 } }, -160, 8, 1875, { .fields = { 64, 144 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 24 } }, -144, 8, 1875, { .fields = { 64, 120 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 16 } }, -128, 8, 1875, { .fields = { 48, 16 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -112, 8, 1875, { .fields = { 0, 240 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 16 } }, -96, 8, 1875, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -80, 8, 1875, { .fields = { 0, 240 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 16 } }, -64, 8, 1875, { .fields = { 16, 240 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 16 } }, -48, 8, 1875, { .fields = { 64, 240 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 16 } }, -32, 8, 1875, { .fields = { 48, 160 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 16 } }, -16, 8, 1875, { .fields = { 48, 64 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 16 } }, 0, 8, 1875, { .fields = { 48, 32 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 16 } }, 16, 8, 1875, { .fields = { 48, 80 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 16 } }, 32, 8, 1875, { .fields = { 48, 96 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 16 } }, 48, 8, 1875, { .fields = { 48, 176 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 16 } }, 64, 8, 1875, { .fields = { 48, 112 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 16 } }, 80, 8, 1875, { .fields = { 48, 192 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 16 } }, 96, 8, 1875, { .fields = { 48, 48 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 24 } }, 112, 8, 1875, { .fields = { 72, 0 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 24 } }, 128, 8, 1875, { .fields = { 64, 96 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 24 } }, 144, 8, 1875, { .fields = { 64, 48 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 32 } }, -160, -104, 1875, { .fields = { 96, 208 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -144, -104, 1875, { .fields = { 0, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, -128, -104, 1875, { .fields = { 0, 120 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 40 } }, -112, -104, 1875, { .fields = { 112, 40 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -96, -104, 1875, { .fields = { 32, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -80, -104, 1875, { .fields = { 40, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -64, -104, 1875, { .fields = { 48, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -48, -104, 1875, { .fields = { 32, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -32, -104, 1875, { .fields = { 96, 200 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, -16, -104, 1875, { .fields = { 88, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, 0, -104, 1875, { .fields = { 48, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, 16, -104, 1875, { .fields = { 48, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, 32, -104, 1875, { .fields = { 64, 144 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, 48, -104, 1875, { .fields = { 48, 48 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, 64, -104, 1875, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, 80, -104, 1875, { .fields = { 64, 96 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 48 } }, 96, -104, 1875, { .fields = { 64, 192 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 40 } }, 112, -104, 1875, { .fields = { 0, 160 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 40 } }, 128, -104, 1875, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 141, 0x3FC0, { .fields = { 16, 32 } }, 144, -104, 1875, { .fields = { 96, 176 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 8, 80 } }, -160, -72, 1875, { .fields = { 120, 0 } }, 128, 128, 128, 0 },
    { 142, 0x3FC0, { .fields = { 16, 64 } }, -40, -56, 1875, { .fields = { 112, 152 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 64 } }, 24, -56, 1875, { .fields = { 120, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 64 } }, 80, -56, 1875, { .fields = { 96, 192 } }, 128, 128, 128, 0 },
    { 141, 0x4000, { .fields = { 16, 40 } }, -160, -72, 3750, { .fields = { 96, 40 } }, 128, 128, 128, 0 },
    { 141, 0x4000, { .fields = { 16, 40 } }, -144, -72, 3750, { .fields = { 112, 80 } }, 128, 128, 128, 0 },
    { 141, 0x4000, { .fields = { 16, 32 } }, -128, -64, 3750, { .fields = { 80, 32 } }, 128, 128, 128, 0 },
    { 141, 0x4000, { .fields = { 16, 32 } }, -112, -64, 3750, { .fields = { 80, 224 } }, 128, 128, 128, 0 },
    { 141, 0x4000, { .fields = { 16, 40 } }, -96, -72, 3750, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 16, 48 } }, -80, -80, 3750, { .fields = { 80, 144 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 16, 48 } }, -64, -80, 3750, { .fields = { 96, 104 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 16, 48 } }, -48, -80, 3750, { .fields = { 96, 56 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 16, 48 } }, -32, -80, 3750, { .fields = { 96, 152 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 16, 48 } }, -16, -80, 3750, { .fields = { 80, 48 } }, 128, 128, 128, 0 },
    { 141, 0x4000, { .fields = { 16, 40 } }, 0, -72, 3750, { .fields = { 112, 120 } }, 128, 128, 128, 0 },
    { 141, 0x4000, { .fields = { 16, 24 } }, 16, -56, 3750, { .fields = { 64, 192 } }, 128, 128, 128, 0 },
    { 141, 0x4000, { .fields = { 16, 24 } }, 32, -56, 3750, { .fields = { 64, 216 } }, 128, 128, 128, 0 },
    { 141, 0x4000, { .fields = { 16, 16 } }, 48, -48, 3750, { .fields = { 112, 240 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 80 } }, 144, -112, 3750, { .fields = { 16, 160 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 16, 72 } }, 128, -104, 3750, { .fields = { 112, 80 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 64 } }, 112, -96, 3750, { .fields = { 80, 192 } }, 128, 128, 128, 0 },
    { 142, 0x4000, { .fields = { 16, 56 } }, 96, -88, 3750, { .fields = { 104, 0 } }, 128, 128, 128, 0 },
    { 141, 0x4000, { .fields = { 16, 40 } }, 80, -72, 3750, { .fields = { 112, 200 } }, 128, 128, 128, 0 },
    { 143, 0x4040, { .fields = { 16, 96 } }, -160, -80, 1900, { .fields = { 112, 96 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4040, { .fields = { 16, 96 } }, -144, -80, 1900, { .fields = { 80, 96 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4040, { .fields = { 16, 96 } }, -128, -80, 1900, { .fields = { 80, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4040, { .fields = { 16, 88 } }, -112, -72, 1900, { .fields = { 32, 88 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4040, { .fields = { 16, 88 } }, -96, -72, 1900, { .fields = { 32, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4040, { .fields = { 16, 88 } }, -80, -72, 1900, { .fields = { 48, 88 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4040, { .fields = { 16, 80 } }, -64, -64, 1900, { .fields = { 0, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4040, { .fields = { 16, 80 } }, -48, -64, 1900, { .fields = { 0, 80 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4040, { .fields = { 16, 80 } }, -32, -64, 1900, { .fields = { 32, 176 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4040, { .fields = { 16, 80 } }, -16, -64, 1900, { .fields = { 16, 80 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4040, { .fields = { 16, 80 } }, 0, -64, 1900, { .fields = { 16, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4040, { .fields = { 16, 80 } }, 16, -64, 1900, { .fields = { 64, 176 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4040, { .fields = { 16, 80 } }, 32, -64, 1900, { .fields = { 0, 160 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4040, { .fields = { 16, 80 } }, 48, -64, 1900, { .fields = { 48, 176 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4040, { .fields = { 16, 88 } }, 64, -72, 1900, { .fields = { 48, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4040, { .fields = { 16, 88 } }, 80, -72, 1900, { .fields = { 64, 88 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4040, { .fields = { 16, 88 } }, 96, -72, 1900, { .fields = { 64, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4040, { .fields = { 16, 96 } }, 112, -80, 1900, { .fields = { 96, 96 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4040, { .fields = { 16, 96 } }, 128, -80, 1900, { .fields = { 112, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4040, { .fields = { 16, 96 } }, 144, -80, 1900, { .fields = { 96, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
};

SpriteBatch D_neo_ark_submarine_gallery_80184890[7] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 20, 0, 0, { 0, 0 } },
    { 20, 20, 0, 0, { 3, 0 } },
    { 40, 44, 0, 0, { 2, 0 } },
    { 84, 19, 0, 0, { 4, 0 } },
    { 103, 20, 0, 0, { 1, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_neo_ark_submarine_gallery_801848C8[19] = {
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -160, 80, 500, { .fields = { 112, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -144, 80, 500, { .fields = { 112, 40 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -128, 80, 500, { .fields = { 112, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -112, 80, 500, { .fields = { 112, 120 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -96, 88, 500, { .fields = { 112, 200 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -80, 80, 500, { .fields = { 112, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -64, 88, 500, { .fields = { 96, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -48, 88, 500, { .fields = { 96, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -32, 88, 500, { .fields = { 96, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, -16, 104, 500, { .fields = { 96, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 0, 112, 500, { .fields = { 88, 224 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 24, 104, 500, { .fields = { 96, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 40, 104, 500, { .fields = { 96, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 56, 104, 500, { .fields = { 96, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 72, 104, 500, { .fields = { 96, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 88, 104, 500, { .fields = { 96, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 104, 104, 500, { .fields = { 96, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 16 } }, 120, 104, 500, { .fields = { 96, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 136, 96, 500, { .fields = { 104, 232 } }, 128, 128, 128, 0 },
};

SpriteBatch D_neo_ark_submarine_gallery_80184A44[3] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 19, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_neo_ark_submarine_gallery_80184A5C[33] = {
    { 143, 0x3FC0, { .fields = { 16, 64 } }, -136, -16, 1125, { .fields = { 48, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, -120, -16, 1125, { .fields = { 32, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, -104, -16, 1125, { .fields = { 32, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, -88, -16, 1125, { .fields = { 32, 192 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 64 } }, -72, -16, 1125, { .fields = { 32, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -56, -16, 1125, { .fields = { 16, 168 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 56 } }, -40, -16, 1125, { .fields = { 8, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 40 } }, -24, 0, 1125, { .fields = { 112, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 32 } }, -8, 0, 1125, { .fields = { 96, 208 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 64 } }, -160, -48, 1000, { .fields = { 48, 128 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 16, 72 } }, -160, -120, 1500, { .fields = { 112, 72 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 16, 72 } }, -144, -120, 1500, { .fields = { 112, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 16, 72 } }, -128, -120, 1500, { .fields = { 112, 144 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 16, 72 } }, -112, -120, 1500, { .fields = { 96, 72 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 16, 72 } }, -96, -120, 1500, { .fields = { 96, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 16, 56 } }, -96, -48, 1000, { .fields = { 8, 56 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 16, 56 } }, -112, -48, 1000, { .fields = { 0, 168 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 16, 64 } }, -80, -120, 1500, { .fields = { 80, 64 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 16, 64 } }, -80, -56, 1000, { .fields = { 96, 144 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 16, 56 } }, -128, -48, 1000, { .fields = { 0, 112 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 16, 64 } }, -144, -48, 1000, { .fields = { 64, 192 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 16, 64 } }, -64, -120, 1500, { .fields = { 48, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 16, 64 } }, -64, -56, 1000, { .fields = { 64, 128 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 16, 64 } }, -48, -56, 1125, { .fields = { 64, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 16, 64 } }, -48, -120, 1500, { .fields = { 64, 64 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 16, 64 } }, -32, -120, 1500, { .fields = { 80, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 16, 64 } }, -32, -56, 1125, { .fields = { 80, 192 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 16, 64 } }, -16, -64, 1500, { .fields = { 80, 128 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 16, 56 } }, -16, -120, 1500, { .fields = { 16, 112 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 64 } }, 8, -64, 2664, { .fields = { 56, 192 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 64 } }, 0, -64, 2717, { .fields = { 48, 192 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 56 } }, 0, -120, 2584, { .fields = { 24, 56 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
    { 143, 0x4000, { .fields = { 8, 56 } }, 8, -120, 2560, { .fields = { 24, 0 } }, 128, 128, 128, SPRITE_SOURCE_SEMI_TRANSPARENT },
};

SpriteBatch D_neo_ark_submarine_gallery_80184CF0[4] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 9, 0, 0, { 1, 0 } },
    { 9, 24, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView D_neo_ark_submarine_gallery_80184D10[7] = {
    { { .empty = D_neo_ark_submarine_gallery_801824BC }, D_neo_ark_submarine_gallery_801824BC, NULL },
    { { .elements = D_neo_ark_submarine_gallery_801824CC }, D_neo_ark_submarine_gallery_80183070, NULL },
    { { .elements = D_neo_ark_submarine_gallery_801830A0 }, D_neo_ark_submarine_gallery_801836B8, NULL },
    { { .elements = D_neo_ark_submarine_gallery_801836E0 }, D_neo_ark_submarine_gallery_80183EC4, NULL },
    { { .elements = D_neo_ark_submarine_gallery_80183EF4 }, D_neo_ark_submarine_gallery_80184890, NULL },
    { { .elements = D_neo_ark_submarine_gallery_801848C8 }, D_neo_ark_submarine_gallery_80184A44, NULL },
    { { .elements = D_neo_ark_submarine_gallery_80184A5C }, D_neo_ark_submarine_gallery_80184CF0, NULL },
};

WorldCoordLight D_neo_ark_submarine_gallery_80184D64[4] = {
    { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, -1000, -1000 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 208, 208, 208 }, { 0, 0 } },
    { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, -1000, 1000 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 208, 208, 208 }, { 0, 0 } },
    { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 1000, -1000, 0 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 208, 208, 208 }, { 0, 0 } },
    { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -1000, -1000, 0 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 208, 208, 208 }, { 0, 0 } },
};

WorldCoordPointLight D_neo_ark_submarine_gallery_80184EC4[10] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -2400, 4608, 1600 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3076, 3076, 3076 }, { 0, 0 } }, 1000, 2000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -2900, 4608, -600 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3076, 3076, 3076 }, { 0, 0 } }, 1000, 2000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -1601, 4608, -2400 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3076, 3076, 3076 }, { 0, 0 } }, 1000, 2000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 600, 4608, -2900 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3076, 3076, 3076 }, { 0, 0 } }, 1000, 2000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 2400, 4608, -1600 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3076, 3076, 3076 }, { 0, 0 } }, 1000, 2000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 2900, 4608, 600 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3076, 3076, 3076 }, { 0, 0 } }, 1000, 2000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 1600, 4608, 2500 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3076, 3076, 3076 }, { 0, 0 } }, 1000, 2000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -600, 4608, 2800 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3076, 3076, 3076 }, { 0, 0 } }, 1000, 2000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 6165, 0 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 413, 413, 618 }, { 0, 0 } }, 1568, 1940 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 1100, 4608, 3300 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3076, 3076, 3076 }, { 0, 0 } }, 1000, 2000 },
};

WorldCoordRoomLights D_neo_ark_submarine_gallery_80185284 = { ARRAY_SIZE(D_neo_ark_submarine_gallery_80184D64), D_neo_ark_submarine_gallery_80184D64, ARRAY_SIZE(D_neo_ark_submarine_gallery_80184EC4), D_neo_ark_submarine_gallery_80184EC4, 0, NULL };

WorldCollisionTrigger D_neo_ark_submarine_gallery_8018529C[8] = {
    { NULL, NULL, NULL, { 1901, 1952, -1890, 0 }, { { 1807, -3712, -1820, 0 }, { -1807, -3712, 1819, 0 }, { 1807, 3712, -1820, 0 }, { -1807, 3712, 1819, 0 } }, { 2912, 0, 2892, 0 }, { 0, 0, 4096, 0 }, 4492, 0, 5, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 1983, 1952, -1761, 0 }, { { -1813, -3712, 1813, 0 }, { 1813, -3712, -1814, 0 }, { -1813, 3712, 1813, 0 }, { 1813, 3712, -1814, 0 } }, { -2904, 0, -2903, 0 }, { 0, 0, 4096, 0 }, 4492, 0, 4, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 1854, 1888, 1791, 0 }, { { -1819, -3712, -1808, 0 }, { 1819, -3712, 1807, 0 }, { -1819, 3712, -1808, 0 }, { 1819, 3712, 1807, 0 } }, { 2893, 0, -2913, 0 }, { 0, 0, 4096, 0 }, 4492, 0, 5, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 2014, 1888, 1791, 0 }, { { 1819, -3712, 1807, 0 }, { -1819, -3712, -1808, 0 }, { 1819, 3712, 1807, 0 }, { -1819, 3712, -1808, 0 } }, { -2895, 0, 2911, 0 }, { 0, 0, 4096, 0 }, 4492, 0, 2, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -2081, 2016, 1696, 0 }, { { 1977, -3712, -1634, 0 }, { -1977, -3712, 1633, 0 }, { 1977, 3712, -1634, 0 }, { -1977, 3712, 1633, 0 } }, { 2615, 0, 3164, 0 }, { 0, 0, 4096, 0 }, 4492, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -1922, 1984, 1727, 0 }, { { -1928, -3712, 1690, 0 }, { 1929, -3712, -1691, 0 }, { -1928, 3712, 1690, 0 }, { 1929, 3712, -1691, 0 } }, { -2707, 0, -3088, 0 }, { 0, 0, 4096, 0 }, 4492, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -1859, 1888, -1762, 0 }, { { -1821, -3712, -1804, 0 }, { 1822, -3712, 1805, 0 }, { -1821, 3712, -1804, 0 }, { 1822, 3712, 1805, 0 } }, { 2888, 0, -2917, 0 }, { 0, 0, 4096, 0 }, 4492, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -1825, 1792, -1920, 0 }, { { 1821, -3712, 1804, 0 }, { -1822, -3712, -1805, 0 }, { 1821, 3712, 1804, 0 }, { -1822, 3712, -1805, 0 } }, { -2890, 0, 2915, 0 }, { 0, 0, 4096, 0 }, 4492, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionTrigger D_neo_ark_submarine_gallery_801854FC[10] = {
    { NULL, NULL, NULL, { 1567, 4944, 3199, 0 }, { { -1224, 0, 11, 0 }, { 479, 0, -1127, 0 }, { -478, 0, 1128, 0 }, { 1225, 0, -10, 0 } }, { 0, 4112, 0, 0 }, { -2440, 0, -3290, 0 }, 1221, WORLD_COLLISION_TRIGGER_ACTION_WARP, 14, 18, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 0, 4960, 0, 0 }, { { -6696, 0, 139, 0 }, { 127, 0, -6951, 0 }, { -126, 0, 6952, 0 }, { 6697, 0, -138, 0 } }, { 0, 4096, 0, 0 }, { -2440, 0, -3290, 0 }, 6945, WORLD_COLLISION_TRIGGER_ACTION_CAP, 3, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 1200, 4928, 3520, 0 }, { { -1976, 0, 427, 0 }, { 1199, 0, -1511, 0 }, { -1230, 0, 1544, 0 }, { 2009, 0, -458, 0 } }, { 0, 4108, 0, 0 }, { -2106, 0, -3513, 0 }, 2048, WORLD_COLLISION_TRIGGER_ACTION_CAP, 1, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -33, 4928, -3488, 0 }, { { -2136, 0, -560, 0 }, { 2072, 0, -782, 0 }, { -2071, 0, 782, 0 }, { 2137, 0, 560, 0 } }, { 0, 4105, 0, 0 }, { 402, 0, 4076, 0 }, 2202, WORLD_COLLISION_TRIGGER_ACTION_CAP, 7, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 3456, 4928, 1447, 0 }, { { -1077, 0, 626, 0 }, { 85, 0, -1461, 0 }, { -245, 0, 1605, 0 }, { 1237, 0, -769, 0 } }, { 0, 4111, 0, 0 }, { -3974, 0, -995, 0 }, 1619, WORLD_COLLISION_TRIGGER_ACTION_CAP, 2, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 3871, 4896, -385, 0 }, { { -574, 0, 1224, 0 }, { -767, 0, -1157, 0 }, { 768, 0, 1158, 0 }, { 575, 0, -1222, 0 } }, { 0, 4104, 0, 0 }, { -4093, 0, 201, 0 }, 1384, WORLD_COLLISION_TRIGGER_ACTION_CAP, 2, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 2656, 4928, -2273, 0 }, { { 332, 0, 1311, 0 }, { -1327, 0, -407, 0 }, { 1328, 0, 408, 0 }, { -331, 0, -1309, 0 } }, { 0, 4104, 0, 0 }, { -2898, 0, 2897, 0 }, 1384, WORLD_COLLISION_TRIGGER_ACTION_CAP, 2, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -2320, 4928, 3024, 0 }, { { 1644, 0, 1443, 0 }, { -2158, 0, -201, 0 }, { 2158, 0, 201, 0 }, { -1643, 0, -1442, 0 } }, { 0, 4108, 0, 0 }, { 3036, 0, -2752, 0 }, 2187, WORLD_COLLISION_TRIGGER_ACTION_CAP, 2, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -3489, 4960, 735, 0 }, { { -286, 0, 2169, 0 }, { -1031, 0, -1906, 0 }, { 1032, 0, 1907, 0 }, { 287, 0, -2167, 0 } }, { 0, 4105, 0, 0 }, { 4053, 0, -602, 0 }, 2187, WORLD_COLLISION_TRIGGER_ACTION_CAP, 2, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { -3169, 4896, -1632, 0 }, { { -1575, 0, 924, 0 }, { 496, 0, -1725, 0 }, { -495, 0, 1725, 0 }, { 1575, 0, -923, 0 } }, { 0, 4102, 0, 0 }, { 3614, 0, 1930, 0 }, 1823, WORLD_COLLISION_TRIGGER_ACTION_CAP, 2, WORLD_COLLISION_TRIGGER_CAP_ROOM_MESSAGE, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

AreaResource D_neo_ark_submarine_gallery_801857F4[3] = {
    { 4, 4, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, &D_actor_100400_80147E48 },
    { 61, 61, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, D_actor_206100_80158B0C },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_neo_ark_submarine_gallery_80185818[2] = {
    { 4, 4, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, &D_actor_100400_80147E48 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_neo_ark_submarine_gallery_80185830[2] = {
    { 4, 4, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, &D_actor_100400_80147E48 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_neo_ark_submarine_gallery_80185848[2] = {
    { 4, 4, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, &D_actor_100400_80147E48 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaVariant D_neo_ark_submarine_gallery_80185860[13] = {
    { NULL, NULL },
    { D_map_neo_ark_8017C4A0, D_neo_ark_submarine_gallery_801857F4 },
    { D_map_neo_ark_8017C4D0, D_neo_ark_submarine_gallery_80185818 },
    { D_map_neo_ark_8017C510, D_neo_ark_submarine_gallery_80185830 },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { D_map_neo_ark_8017C530, D_neo_ark_submarine_gallery_80185848 },
    { NULL, NULL },
};

WorldCollisionFootstepSounds D_neo_ark_submarine_gallery_801858C8 = {
    0x10000015,
    0x10000017,
    0x10000015,
};

WorldCollisionSurfaceProperties D_neo_ark_submarine_gallery_801858D4[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_neo_ark_submarine_gallery_801858DC[1] = {
    { 0, WORLD_COLLISION_SURFACE_PASS_PROBES, WORLD_COLLISION_SURFACE_IGNORE_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_neo_ark_submarine_gallery_801858E4[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_neo_ark_submarine_gallery_801858C8 },
};

WorldCollisionSurfaceProperties* D_neo_ark_submarine_gallery_801858EC[8] = {
    D_neo_ark_submarine_gallery_801858D4,
    D_neo_ark_submarine_gallery_801858D4,
    D_neo_ark_submarine_gallery_801858D4,
    D_neo_ark_submarine_gallery_801858DC,
    D_neo_ark_submarine_gallery_801858E4,
    D_neo_ark_submarine_gallery_801858D4,
    D_neo_ark_submarine_gallery_801858D4,
    D_neo_ark_submarine_gallery_801858D4,
};

static void _glowDrawCapsule(const SVECTOR worldPoints[2], s32 radiusScale, s32 packedColor);

/// Per-view draw callback for the gallery's display cases. The first state
/// latches the two effect ids the display cases animate with; every later run
/// draws one fixed set of positions for the current camera view. Views 2 to 6
/// each cover a run of `D_neo_ark_submarine_gallery_801818*` entries, and view 2
/// also hands the task's own coordinate to `_neoArkSubmarineGalleryDrawLightPrism`.
void func_neo_ark_submarine_gallery_8017EFEC(Task* arg0)
{
    SVECTOR*  pos;
    GfxCoord* coord;
    s32       view;

    coord = arg0->extra.coordBody->coord;
    if (arg0->state == 0) {
        gRoomEffectWaterRippleId = EFFECT_NEO_ARK_SUBMARINE_GALLERY_WATER_RIPPLE;
        gRoomEffectWaterSprayId  = EFFECT_NEO_ARK_SUBMARINE_GALLERY_WATER_SPRAY;
        arg0->state              = 1;
    }
    view = viewGetMappedIndex() & 0xFF;
    switch (view) {
        case 2:
            _glowDrawCapsule(&D_neo_ark_submarine_gallery_801818C8[0], 0x200, 0x444);
            _glowDrawCapsule(&D_neo_ark_submarine_gallery_801818C8[2], 0x200, 0x444);
            _glowDrawCapsule(&D_neo_ark_submarine_gallery_801818C8[4], 0x200, 0x444);
            glowDrawDisc(&D_neo_ark_submarine_gallery_801818C8[16], 0x200, 0x444);
            glowDrawDisc(&D_neo_ark_submarine_gallery_801818C8[17], 0x200, 0x444);
            glowDrawDisc(&D_neo_ark_submarine_gallery_801818C8[18], 0x200, 0x444);
            glowDrawDisc(&D_neo_ark_submarine_gallery_801818C8[31], 0x200, 0x444);
            _neoArkSubmarineGalleryDrawLightPrism(coord, NEO_ARK_SUBMARINE_GALLERY_PRISM_FIRST_VERTEX);
            break;
        case 3:
            _glowDrawCapsule(&D_neo_ark_submarine_gallery_80181928[0], 0x200, 0x444);
            glowDrawDisc(&D_neo_ark_submarine_gallery_80181928[4], 0x200, 0x444);
            glowDrawDisc(&D_neo_ark_submarine_gallery_80181928[5], 0x200, 0x444);
            glowDrawDisc(&D_neo_ark_submarine_gallery_80181928[14], 0x200, 0x444);
            glowDrawDisc(&D_neo_ark_submarine_gallery_80181928[15], 0x200, 0x444);
            glowDrawDisc(&D_neo_ark_submarine_gallery_80181928[16], 0x200, 0x444);
            glowDrawDisc(&D_neo_ark_submarine_gallery_80181928[17], 0x200, 0x444);
            glowDrawDisc(&D_neo_ark_submarine_gallery_80181928[18], 0x200, 0x444);
            pos = &D_neo_ark_submarine_gallery_80181928[19];
            glowDrawDisc(pos, 0x200, 0x444);
            break;
        case 4:
            _glowDrawCapsule(&D_neo_ark_submarine_gallery_801818F8[0], 0x200, 0x444);
            _glowDrawCapsule(&D_neo_ark_submarine_gallery_801818F8[2], 0x200, 0x444);
            _glowDrawCapsule(&D_neo_ark_submarine_gallery_801818F8[4], 0x200, 0x444);
            glowDrawDisc(&D_neo_ark_submarine_gallery_801818F8[18], 0x200, 0x444);
            glowDrawDisc(&D_neo_ark_submarine_gallery_801818F8[19], 0x200, 0x444);
            glowDrawDisc(&D_neo_ark_submarine_gallery_801818F8[20], 0x200, 0x444);
            pos = &D_neo_ark_submarine_gallery_801818F8[21];
            glowDrawDisc(pos, 0x200, 0x444);
            break;
        case 5:
            _glowDrawCapsule(&D_neo_ark_submarine_gallery_801818D8[0], 0x200, 0x444);
            _glowDrawCapsule(&D_neo_ark_submarine_gallery_801818D8[2], 0x200, 0x444);
            pos = &D_neo_ark_submarine_gallery_801818D8[3];
            _glowDrawCapsule(pos, 0x200, 0x444);
            break;
        case 6:
            _glowDrawCapsule(&D_neo_ark_submarine_gallery_801818F8[0], 0x200, 0x444);
            _glowDrawCapsule(&D_neo_ark_submarine_gallery_801818F8[2], 0x200, 0x444);
            pos = &D_neo_ark_submarine_gallery_801818F8[4];
            _glowDrawCapsule(pos, 0x200, 0x444);
            break;
    }
}

#include "../../shared/water_ripple_task.inc.c"

void neoArkSubmarineGalleryWaterRippleTask(Task* task)
{
    _waterRippleTask(task);
}

#include "../../shared/water_splash.inc.c"

#include "../../shared/water_drift_task_u16.inc.c"

void neoArkSubmarineGalleryWaterDriftTaskU16(Task* task)
{
    _waterDriftTaskU16(task);
}

#include "../../shared/water_spin_u16.inc.c"

#include "../../shared/water_tile_u16.inc.c"

#define GLOW_DRAW_CAPSULE_PULL 0x40
#include "../../shared/glow_draw_capsule.inc.c"

#define GLOW_DRAW_DISC_PULL 0x40
#include "../../shared/glow_draw_disc.inc.c"

/// Projects one face of the gallery's light prism into screen pixels.
///
/// `quadScratch` borrows a live, writable, word-aligned `EffectQuadScratch`.
/// Initialize all four vertices' XYZ components as signed 16-bit positions
/// in `GsWSMATRIX`'s input space, in GPU quad strip order. Loads that matrix's
/// Q12 rotation; its translation and the GTE projection settings must already
/// be loaded.
///
/// Writes all four `screenCorners` and the final RTPT `projectionFlags`, even
/// when projection fails. Corner 0's RTPS flags are discarded; the caller
/// rejects a negative final FLAG. Leaves `vertices` and `depth` untouched,
/// with corner 3's depth in GTE SZ3 for reading before another depth-changing
/// GTE command. GTE state is not restored. Reserves no storage and retains no
/// pointer.
static inline void _neoArkSubmarineGalleryProjectPrismQuad(EffectQuadScratch* quadScratch)
{
    gte_SetRotMatrix(&GsWSMATRIX);
    // Save corner 0 before the triple transform replaces the screen FIFO.
    gte_ldv0(&quadScratch->vertices[0]);
    gte_rtps();
    gte_stsxy(&quadScratch->screenCorners[0]);
    gte_ldv3(&quadScratch->vertices[1], &quadScratch->vertices[2], &quadScratch->vertices[3]);
    gte_rtpt();
    gte_stsxy3(&quadScratch->screenCorners[1], &quadScratch->screenCorners[2], &quadScratch->screenCorners[3]);
    gte_stflg(&quadScratch->projectionFlags);
}

/// Draws a pulsing additive light prism with four fading sides and a lit cap.
///
/// `firstVertex` selects eight consecutive entries in the room's corner table;
/// the only caller selects entries 32..39. The first four form the lit ring,
/// the next four its dark counterpart. `coord` is borrowed read-only with
/// `workm` already composed in the input space of `GsWSMATRIX`. Rotation is
/// Q12; rotated corners and translated positions retain their low 16 bits as
/// signed GTE coordinates. Brightness is 22..26 on a four-frame sine cycle.
///
/// Each nonnegative final RTPT FLAG emits one `POLY_G4`, ordered by its last
/// corner's SZ3 / 4; the preceding RTPS FLAG is discarded. Requires room for
/// five quad packets and their blend commands in the frame primitive arena,
/// and one word-aligned `EffectQuadScratch` on the initialized scratch stack.
/// The scratch block is released on return; no pointer is retained and GTE
/// state is overwritten.
static void _neoArkSubmarineGalleryDrawLightPrism(const GfxCoord* coord, s16 firstVertex)
{
    /// Rotates and translates one prism corner into the scratch quad.
    ///
    /// Captures `coord` (borrowed, composed const GfxCoord*) and `quadScratch`
    /// (live EffectQuadScratch*). The GTE rotation must already be `coord->workm`.
    /// Arguments have no side effects: vertexIndex is a table index evaluated
    /// once; cornerSlot is 0..3 and evaluated repeatedly. Each axis retains its
    /// low 16 bits as a signed projection coordinate. Use only as a standalone
    /// statement sequence here, never as an unbraced branch body.
#define NEO_ARK_SUBMARINE_GALLERY_TRANSFORM_PRISM_CORNER(vertexIndex, cornerSlot)                                  \
    gte_ldv0(&D_neo_ark_submarine_gallery_801818C8[(vertexIndex)]);                                                \
    gte_rtv0();                                                                                                    \
    gte_stsv(&quadScratch->vertices[(cornerSlot)]);                                                                \
    quadScratch->vertices[(cornerSlot)].vx = (u16)quadScratch->vertices[(cornerSlot)].vx + (u16)coord->workm.t[0]; \
    quadScratch->vertices[(cornerSlot)].vy = (u16)quadScratch->vertices[(cornerSlot)].vy + (u16)coord->workm.t[1]; \
    quadScratch->vertices[(cornerSlot)].vz = (u16)quadScratch->vertices[(cornerSlot)].vz + (u16)coord->workm.t[2]

    enum {
        PRISM_RING_CORNERS          = 4,
        PRISM_PULSE_ANGLE_SHIFT     = 10,
        PRISM_PULSE_INTENSITY_SHIFT = 11,
        PRISM_BASE_INTENSITY        = 24,
    };

    EffectQuadScratch* quadScratch;
    POLY_G4*           quad;
    s32                cornerIndex;
    s32                nextCorner;
    s32                farCorner;
    s32                farNextCorner;
    u8                 intensity;

    SCRATCH_STACK_RESERVE_BLOCK(EffectQuadScratch);
    quadScratch = SCRATCH_STACK_CURSOR(EffectQuadScratch);
    gte_SetTransMatrix(&GsWSMATRIX);
    intensity = (rsin(gDisplayState.animFrame << PRISM_PULSE_ANGLE_SHIFT) >> PRISM_PULSE_INTENSITY_SHIFT) + PRISM_BASE_INTENSITY;
    // Join each adjacent pair of lit corners to the corresponding dark pair.
    for (cornerIndex = 0; cornerIndex < PRISM_RING_CORNERS; cornerIndex++) {
        gte_SetRotMatrix(&coord->workm);
        NEO_ARK_SUBMARINE_GALLERY_TRANSFORM_PRISM_CORNER(firstVertex + cornerIndex, 0);
        gte_SetRotMatrix(&coord->workm);
        nextCorner = (cornerIndex + 1) & (PRISM_RING_CORNERS - 1);
        NEO_ARK_SUBMARINE_GALLERY_TRANSFORM_PRISM_CORNER(firstVertex + nextCorner, 1);
        gte_SetRotMatrix(&coord->workm);
        farCorner = cornerIndex + PRISM_RING_CORNERS;
        NEO_ARK_SUBMARINE_GALLERY_TRANSFORM_PRISM_CORNER(firstVertex + farCorner, 2);
        gte_SetRotMatrix(&coord->workm);
        farNextCorner = nextCorner + PRISM_RING_CORNERS;
        NEO_ARK_SUBMARINE_GALLERY_TRANSFORM_PRISM_CORNER(firstVertex + farNextCorner, 3);
        _neoArkSubmarineGalleryProjectPrismQuad(quadScratch);
        if (quadScratch->projectionFlags >= 0) {
            gte_stszotz(&quadScratch->depth);
            quad           = gGpuPrimCursor;
            gGpuPrimCursor = quad + 1;
            setPolyG4(quad);
            setRGB0(quad, intensity, intensity, intensity);
            setRGB1(quad, intensity, intensity, intensity);
            setRGB2(quad, 0, 0, 0);
            setRGB3(quad, 0, 0, 0);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET((((u32)(quadScratch->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    quad);
            quad->x0 = quadScratch->screenCorners[0].vx;
            quad->y0 = quadScratch->screenCorners[0].vy;
            quad->x1 = quadScratch->screenCorners[1].vx;
            quad->y1 = quadScratch->screenCorners[1].vy;
            quad->x2 = quadScratch->screenCorners[2].vx;
            quad->y2 = quadScratch->screenCorners[2].vy;
            quad->x3 = quadScratch->screenCorners[3].vx;
            quad->y3 = quadScratch->screenCorners[3].vy;
            gpuSetPrimitiveBlendMode(quad, GPU_BLEND_ADD, quadScratch->depth);
        }
    }
    // Close the lit ring in the GPU quad's strip order, 0, 1, 3, 2.
    gte_SetRotMatrix(&coord->workm);
    NEO_ARK_SUBMARINE_GALLERY_TRANSFORM_PRISM_CORNER(firstVertex, 0);
    gte_SetRotMatrix(&coord->workm);
    NEO_ARK_SUBMARINE_GALLERY_TRANSFORM_PRISM_CORNER(firstVertex + 1, 1);
    gte_SetRotMatrix(&coord->workm);
    NEO_ARK_SUBMARINE_GALLERY_TRANSFORM_PRISM_CORNER(firstVertex + 3, 2);
    gte_SetRotMatrix(&coord->workm);
    NEO_ARK_SUBMARINE_GALLERY_TRANSFORM_PRISM_CORNER(firstVertex + 2, 3);
    _neoArkSubmarineGalleryProjectPrismQuad(quadScratch);
    if (quadScratch->projectionFlags >= 0) {
        gte_stszotz(&quadScratch->depth);
        quad           = gGpuPrimCursor;
        gGpuPrimCursor = quad + 1;
        setPolyG4(quad);
        setRGB0(quad, intensity, intensity, intensity);
        setRGB1(quad, intensity, intensity, intensity);
        setRGB2(quad, intensity, intensity, intensity);
        setRGB3(quad, intensity, intensity, intensity);
        quad->x0 = quadScratch->screenCorners[0].vx;
        quad->y0 = quadScratch->screenCorners[0].vy;
        quad->x1 = quadScratch->screenCorners[1].vx;
        quad->y1 = quadScratch->screenCorners[1].vy;
        quad->x2 = quadScratch->screenCorners[2].vx;
        quad->y2 = quadScratch->screenCorners[2].vy;
        quad->x3 = quadScratch->screenCorners[3].vx;
        quad->y3 = quadScratch->screenCorners[3].vy;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET((((u32)(quadScratch->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)), quad);
        gpuSetPrimitiveBlendMode(quad, GPU_BLEND_ADD, quadScratch->depth);
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectQuadScratch);
#undef NEO_ARK_SUBMARINE_GALLERY_TRANSFORM_PRISM_CORNER
}
