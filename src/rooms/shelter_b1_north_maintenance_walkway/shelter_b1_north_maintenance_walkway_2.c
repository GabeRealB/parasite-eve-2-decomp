#include "rooms/shelter_b1_north_maintenance_walkway.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "common.h"
#include "gte.h"

#include "shelter_b1_north_maintenance_walkway_private.h"

#include "actors/task_tables.h"

#include "gameplay/actor_render.h"
#include "gameplay/area.h"
#include "gameplay/areaplace.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/effects.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/light.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gameflag_ids.h"
#include "main/gamemain.h"
#include "main/gfx.h"
#include "main/gfx_types.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "rooms/room.h"

#include "rooms/room_common.h"

#include "../../shared/room_visual_effects.h"

#define ROOM_FX_HALO_STORAGE_INITIALIZER { { { 0, 1, 2 }, { 2, 1, 0 }, { 0, 2, 1 } }, 0x374F }
#define ROOM_FX_HALO_STORAGE_TYPE        RoomFxHaloStorage
#define ROOM_FX_HALO_STORAGE_BOUND
#include "../../shared/room_visual_effects_halo_data.inc.c"
#include "../../shared/room_visual_effects_trail_data.inc.c"
#include "../../shared/room_visual_effects_disc_data.inc.c"
#include "../../shared/room_events.h"
#include "../../shared/glow_draw.h"

/// Returns this overlay's three read-only halo tint rows for spawn indices 0..2.
static inline const RoomFxShade* _roomVisualEffectsGetHaloShades(void)
{
    return _gRoomEffectHaloShades.entries;
}
#undef ROOM_FX_HALO_STORAGE_INITIALIZER
#undef ROOM_FX_HALO_STORAGE_TYPE
#undef ROOM_FX_HALO_STORAGE_BOUND

u8* D_shelter_b1_north_maintenance_walkway_80184B80[1] = {
    gViewIdentityMap,
};

ViewCount D_shelter_b1_north_maintenance_walkway_80184B84[1] = { 6 };

DirectionWarpEntry D_shelter_b1_north_maintenance_walkway_80184B88[2] = {
    { { { .word = 0 }, -2000, 0, 3600 }, { 0, 0, 0, 0 }, { { .word = 0 }, -2000, 0, 3600 }, { 0, 0, 0, 0 }, 0x540C0002, 0x540C0001, DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_NONE, GAME_FLAG_MAP_MARK_SHELTER_1B3 },
    { { { .word = 0 }, 1983, 0, -4420 }, { 0, 0, 0, 0 }, { { .word = 0 }, 1983, 0, -4420 }, { 0, 0, 0, 0 }, 0x540C0004, 0x540C0003, DIRECTION_WARP_SOUND_NONE, 5, DIRECTION_WARP_FLAG_NONE, GAME_FLAG_MAP_MARK_SHELTER_1B0 },
};

static SVECTOR _gShelterB1NorthMaintenanceWalkwayCollision07980Normals[14] = {
#include "assets/shelter_b1_north_maintenance_walkway_collision_07980_normals.inc"
};

static SVECTOR _gShelterB1NorthMaintenanceWalkwayCollision07980Verts[38] = {
#include "assets/shelter_b1_north_maintenance_walkway_collision_07980_verts.inc"
};

static WorldCollisionGridFace _gShelterB1NorthMaintenanceWalkwayCollision07980Faces[18] = {
#include "assets/shelter_b1_north_maintenance_walkway_collision_07980_faces.inc"
};

static s16 _gShelterB1NorthMaintenanceWalkwayCollision07980Cells[92] = {
#include "assets/shelter_b1_north_maintenance_walkway_collision_07980_cells.inc"
};

#define GRID_CELL(i) (&_gShelterB1NorthMaintenanceWalkwayCollision07980Cells[i])
static s16* _gShelterB1NorthMaintenanceWalkwayCollision07980Table[6] = {
#include "assets/shelter_b1_north_maintenance_walkway_collision_07980_table.inc"
};
#undef GRID_CELL

WorldCollisionGrid D_shelter_b1_north_maintenance_walkway_80184F40 = { NULL, _gShelterB1NorthMaintenanceWalkwayCollision07980Normals, _gShelterB1NorthMaintenanceWalkwayCollision07980Verts, _gShelterB1NorthMaintenanceWalkwayCollision07980Faces, _gShelterB1NorthMaintenanceWalkwayCollision07980Table, 2872, 4937, 2, 3, 4000, 18 };

ViewCamera D_shelter_b1_north_maintenance_walkway_80184F64[6] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { 0, 0x35D2, 0 } }, 235 },
    { { { { -1505, 0, 3809 }, { 1128, 3912, 446 }, { -3638, 1213, -1437 } }, { -1615, 1990, -4920 } }, 235 },
    { { { { -938, 0, -3987 }, { -839, 4004, 197 }, { 3897, 861, -917 } }, { 2967, 1799, -4953 } }, 235 },
    { { { { -3922, 0, 1178 }, { 358, 3901, 1193 }, { -1122, 1245, -3736 } }, { -2962, 1720, -4953 } }, 225 },
    { { { { -3898, 0, 1255 }, { 321, 3959, 999 }, { -1213, 1049, -3768 } }, { -2910, 1543, -771 } }, 235 },
    { { { { -938, 0, -3987 }, { -839, 4004, 197 }, { 3897, 861, -917 } }, { 2967, 1799, -4953 } }, 235 },
};

SpriteBatch D_shelter_b1_north_maintenance_walkway_8018503C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b1_north_maintenance_walkway_8018504C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteSource D_shelter_b1_north_maintenance_walkway_8018505C[38] = {
    { 143, 0x3FC0, { .fields = { 32, 16 } }, 128, -96, 730, { .fields = { 16, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 16 } }, 80, -96, 764, { .fields = { 8, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, 40, -80, 955, { .fields = { 16, 0 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, 80, -80, 792, { .fields = { 16, 16 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, 120, -80, 778, { .fields = { 8, 80 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 24 } }, 120, -64, 875, { .fields = { 48, 136 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 24 } }, 120, -40, 875, { .fields = { 48, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 24 } }, 120, -16, 875, { .fields = { 48, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 24 } }, 120, 8, 875, { .fields = { 56, 112 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 24 } }, 120, 32, 875, { .fields = { 48, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, 120, 56, 875, { .fields = { 8, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 24 } }, 80, 48, 1075, { .fields = { 48, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 24 } }, 80, 24, 1050, { .fields = { 40, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 24 } }, 80, 0, 975, { .fields = { 40, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 24 } }, 80, -48, 925, { .fields = { 80, 208 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 16 } }, 80, -64, 925, { .fields = { 8, 128 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 40, 8 } }, 40, -64, 955, { .fields = { 8, 176 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 8 } }, 56, -56, 950, { .fields = { 24, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 8 } }, 64, -48, 950, { .fields = { 32, 160 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 72, -40, 975, { .fields = { 112, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 72, -16, 950, { .fields = { 120, 216 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 72, 8, 1075, { .fields = { 112, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 72, 32, 1075, { .fields = { 40, 96 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 32, 24 } }, 48, 48, 1210, { .fields = { 80, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 40, 32, 1100, { .fields = { 88, 64 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 24 } }, 56, 24, 1075, { .fields = { 88, 88 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 16 } }, 48, 32, 1109, { .fields = { 120, 240 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 8, 32 } }, 64, 16, 1075, { .fields = { 120, 184 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 16, 24 } }, 80, -24, 922, { .fields = { 88, 144 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 24, 24 } }, 96, -24, 950, { .fields = { 88, 232 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 48, 16 } }, 32, -96, 945, { .fields = { 8, 32 } }, 128, 128, 128, 0 },
    { 143, 0x3FC0, { .fields = { 72, 64 } }, -40, -24, 5000, { .fields = { 56, 0 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 32, 40 } }, 128, -120, 875, { .fields = { 96, 64 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 24, 40 } }, 104, -120, 900, { .fields = { 104, 144 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 32, 40 } }, 72, -120, 912, { .fields = { 96, 104 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 16 } }, 56, -112, 925, { .fields = { 88, 168 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 16, 8 } }, 40, -104, 925, { .fields = { 32, 168 } }, 128, 128, 128, 0 },
    { 143, 0x4000, { .fields = { 48, 16 } }, 24, -96, 925, { .fields = { 8, 48 } }, 128, 128, 128, 0 },
};

SpriteBatch D_shelter_b1_north_maintenance_walkway_80185354[5] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { 0, 31, 0, 0, { 2, 0 } },
    { 31, 1, 0, 0, { 1, 0 } },
    { 32, 6, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b1_north_maintenance_walkway_8018537C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b1_north_maintenance_walkway_8018538C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b1_north_maintenance_walkway_8018539C[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView D_shelter_b1_north_maintenance_walkway_801853AC[6] = {
    { { .empty = D_shelter_b1_north_maintenance_walkway_8018503C }, D_shelter_b1_north_maintenance_walkway_8018503C, NULL },
    { { .empty = D_shelter_b1_north_maintenance_walkway_8018504C }, D_shelter_b1_north_maintenance_walkway_8018504C, NULL },
    { { .elements = D_shelter_b1_north_maintenance_walkway_8018505C }, D_shelter_b1_north_maintenance_walkway_80185354, NULL },
    { { .empty = D_shelter_b1_north_maintenance_walkway_8018537C }, D_shelter_b1_north_maintenance_walkway_8018537C, NULL },
    { { .empty = D_shelter_b1_north_maintenance_walkway_8018538C }, D_shelter_b1_north_maintenance_walkway_8018538C, NULL },
    { { .empty = D_shelter_b1_north_maintenance_walkway_8018539C }, D_shelter_b1_north_maintenance_walkway_8018539C, NULL },
};

WorldCoordPointLight D_shelter_b1_north_maintenance_walkway_801853F4[5] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -1863, -221, 4731 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2758, 2836, 2918 }, { 0, 0 } }, 948, 2672 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 68, -223, 4315 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2698, 2860, 2900 }, { 0, 0 } }, 1359, 3223 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 2002, -504, -2007 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2785, 2839, 2881 }, { 0, 0 } }, 1799, 3522 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 1916, -2, 445 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2839, 2860, 2999 }, { 0, 0 } }, 1721, 3243 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 1988, -223, 2836 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2659, 2760, 2850 }, { 0, 0 } }, 1741, 3222 },
};

WorldCoordRoomLights D_shelter_b1_north_maintenance_walkway_801855D4 = { 0, NULL, ARRAY_SIZE(D_shelter_b1_north_maintenance_walkway_801853F4), D_shelter_b1_north_maintenance_walkway_801853F4, 0, NULL };

WorldCollisionTrigger D_shelter_b1_north_maintenance_walkway_801855EC[6] = {
    { NULL, NULL, NULL, { 2080, -1584, -576, 0 }, { { -1376, -1904, 192, 0 }, { 1376, -1904, -192, 0 }, { -1376, 1904, 192, 0 }, { 1376, 1904, -192, 0 } }, { -569, 0, -4074, 0 }, { 0, 0, 4096, 0 }, 2346, 0, 5, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 2048, -1600, -672, 0 }, { { 1408, -1904, -192, 0 }, { -1408, -1904, 192, 0 }, { 1408, 1904, -192, 0 }, { -1408, 1904, 192, 0 } }, { 554, 0, 4067, 0 }, { 0, 0, 4096, 0 }, 2374, 0, 4, 5, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 2240, -1504, 3200, 0 }, { { 1536, -1904, 256, 0 }, { -1536, -1904, -256, 0 }, { 1536, 1904, 256, 0 }, { -1536, 1904, -256, 0 } }, { -676, 0, 4053, 0 }, { 0, 0, 4096, 0 }, 2455, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 2176, -1505, 3280, 0 }, { { -1536, -1904, -272, 0 }, { 1536, -1904, 272, 0 }, { -1536, 1904, -272, 0 }, { 1536, 1904, 272, 0 } }, { 714, 0, -4039, 0 }, { 0, 0, 4096, 0 }, 2455, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -608, -1568, 4016, 0 }, { { 96, -1904, -1312, 0 }, { -96, -1904, 1312, 0 }, { 96, 1904, -1312, 0 }, { -96, 1904, 1312, 0 } }, { 4090, 0, 298, 0 }, { 0, 0, 4096, 0 }, 2304, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { -496, -1600, 3984, 0 }, { { -80, -1904, 1328, 0 }, { 80, -1904, -1328, 0 }, { -80, 1904, 1328, 0 }, { 80, 1904, -1328, 0 } }, { -4097, 0, -248, 0 }, { 0, 0, 4096, 0 }, 2318, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionOccluder D_shelter_b1_north_maintenance_walkway_801857B4[1] = {
    { NULL, NULL, { -1280, -1104, 1776, 0 }, { { -1920, 2128, -848, 0 }, { 1920, 2128, 848, 0 }, { -1920, -2128, -848, 0 }, { 1920, -2128, 848, 0 } }, { -1662, 0, 3761, 0 }, 2985, 1 | WORLD_COLLISION_OCCLUDER_LAST, 0 },
};

AreaResource D_shelter_b1_north_maintenance_walkway_801857F0[3] = {
    { 21, 21, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, D_actor_102100_80135C30 },
    { 24, 24, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, D_actor_202400_8014E47C },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_shelter_b1_north_maintenance_walkway_80185814[2] = {
    { 3, 3, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, D_actor_100300_80148110 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_shelter_b1_north_maintenance_walkway_8018582C[4] = {
    { 70, 70, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, D_actor_107000_8013F5F0 },
    { 46, 46, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, &Actor04600_D05878 },
    { 47, 47, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, &Actor04600_D0649C },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_shelter_b1_north_maintenance_walkway_8018585C[2] = {
    { 11, 11, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, D_actor_101100_80147400 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_shelter_b1_north_maintenance_walkway_80185874[3] = {
    { 21, 21, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, D_actor_102100_80135C30 },
    { 57, 57, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, D_actor_205700_801611F8 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaPlacement D_shelter_b1_north_maintenance_walkway_80185898[6] = {
    { 21, 1, 0, -2900, -2000, 4900, 1024, 0, 0, 2, 3 },
    { 21, 1, 0, -2900, -2000, 4000, 1024, 0, 0, 2, 3 },
    { 24, 0, 0, 1500, 0, 2900, 800, 0, 2, 4, 0 },
    { 24, 0, 0, 2500, 0, 2100, 2300, 0, 2, 4, 0 },
    { 24, 0, 0, 1500, 0, -1200, 1900, 0, 2, 4, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaPlacement D_shelter_b1_north_maintenance_walkway_801858F8[2] = {
    { 3, 0, 0, 2000, 0, 4000, 3072, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaPlacement D_shelter_b1_north_maintenance_walkway_80185918[10] = {
    { 70, 0, 0, 1750, 0, -1050, 2400, 0, 0, 2, 0 },
    { 70, 0, 0, 2300, 0, 3600, 3600, 0, 0, 2, 0 },
    { 46, 0, 0, 2550, 0, 4500, 500, 0, 2, 4, 0 },
    { 46, 0, 0, 500, 0, 3600, 3300, 0, 2, 4, 0 },
    { 46, 0, 0, 1850, 0, 2050, 1500, 0, 2, 4, 0 },
    { 47, 0, 0, -1800, 0, 4500, 2100, 0, 2, 4, 0 },
    { 47, 0, 0, -350, 0, 3900, 3050, 0, 2, 4, 0 },
    { 47, 0, 0, 1550, 0, -2000, 1800, 0, 2, 4, 0 },
    { 47, 0, 0, 2550, 0, -2400, 2500, 0, 2, 4, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaPlacement D_shelter_b1_north_maintenance_walkway_801859B8[2] = {
    { 11, 0, 0, 2000, 0, 1600, 0, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaPlacement D_shelter_b1_north_maintenance_walkway_801859D8[6] = {
    { 21, 1, 0, -2900, -2000, 4900, 1024, 0, 0, 2, 3 },
    { 21, 1, 0, -2900, -2000, 4000, 1024, 0, 0, 2, 3 },
    { 21, 1, 0, 2000, -1900, 5100, 2048, 0, 0, 2, 3 },
    { 21, 1, 0, 3200, -1900, 4000, 3072, 0, 0, 2, 3 },
    { 57, 0, 0, -500, 0, 4000, 1024, 0, 2, 4, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaVariant D_shelter_b1_north_maintenance_walkway_80185A38[12] = {
    { NULL, NULL },
    { D_shelter_b1_north_maintenance_walkway_80185898, D_shelter_b1_north_maintenance_walkway_801857F0 },
    { D_shelter_b1_north_maintenance_walkway_801858F8, D_shelter_b1_north_maintenance_walkway_80185814 },
    { D_shelter_b1_north_maintenance_walkway_80185918, D_shelter_b1_north_maintenance_walkway_8018582C },
    { D_shelter_b1_north_maintenance_walkway_801859B8, D_shelter_b1_north_maintenance_walkway_8018585C },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { D_shelter_b1_north_maintenance_walkway_801859D8, D_shelter_b1_north_maintenance_walkway_80185874 },
};

WorldCollisionTrigger D_shelter_b1_north_maintenance_walkway_80185A98[2] = {
    { NULL, NULL, NULL, { -2128, -48, 3344, 0 }, { { -720, 0, -400, 0 }, { 720, 0, -400, 0 }, { -720, 0, 400, 0 }, { 720, 0, 400, 0 } }, { 0, 4098, 0, 0 }, { 0, 0, 4096, 0 }, 822, WORLD_COLLISION_TRIGGER_ACTION_WARP, 14, 18, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 1984, -48, -4560, 0 }, { { -1024, 0, -448, 0 }, { 1024, 0, -448, 0 }, { -1024, 0, 448, 0 }, { 1024, 0, 448, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, 4096, 0 }, 1115, WORLD_COLLISION_TRIGGER_ACTION_WARP, 11, 35, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionFootstepSounds D_shelter_b1_north_maintenance_walkway_80185B30 = {
    0x1000005D,
    0x1000005F,
    0x1000005D,
};

WorldCollisionSurfaceProperties D_shelter_b1_north_maintenance_walkway_80185B3C[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_shelter_b1_north_maintenance_walkway_80185B44[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_shelter_b1_north_maintenance_walkway_80185B30 },
};

WorldCollisionSurfaceProperties* D_shelter_b1_north_maintenance_walkway_80185B4C[8] = {
    D_shelter_b1_north_maintenance_walkway_80185B3C,
    D_shelter_b1_north_maintenance_walkway_80185B44,
    D_shelter_b1_north_maintenance_walkway_80185B3C,
    D_shelter_b1_north_maintenance_walkway_80185B3C,
    D_shelter_b1_north_maintenance_walkway_80185B3C,
    D_shelter_b1_north_maintenance_walkway_80185B3C,
    D_shelter_b1_north_maintenance_walkway_80185B3C,
    D_shelter_b1_north_maintenance_walkway_80185B3C,
};

RoomFadeStorage gRoomEventFade = { 0 };

RoomEventMsg gRoomEventStagedMsg = { 0 };

s8 D_shelter_b1_north_maintenance_walkway_80185B7C[4] = {
    0,
    89,
    61,
    49,
};

RoomLatchedEvent gRoomEventLatched;

#include "../../shared/glow_draw_cone.inc.c"

#include "../../shared/glow_draw_red_disc.inc.c"

#include "../../shared/room_visual_effects.inc.c"

void func_shelter_b1_north_maintenance_walkway_8017E8B8(Task* task)
{
    RoomFx_MoteTask(task);
}

#include "../../shared/room_visual_effects_halo.inc.c"

void func_shelter_b1_north_maintenance_walkway_8017F600(Task* arg0)
{
    _roomVisualEffectsHaloTask(arg0);
}

void func_shelter_b1_north_maintenance_walkway_8017F998(Task* arg0)
{
    _roomVisualEffectsHaloOrangeBurstTask(arg0);
}

#include "../../shared/room_visual_effects_glow_quad.inc.c"
#include "../../shared/room_visual_effects_flash.inc.c"

void func_shelter_b1_north_maintenance_walkway_80180DA8(Task* arg0)
{
    RoomFx_SparkEmitterTask(arg0);
}

#include "../../shared/room_visual_effects_flash_task.inc.c"

void func_shelter_b1_north_maintenance_walkway_80180EDC(Task* arg0)
{
    _roomVisualEffectsFlashTask(arg0);
}

#include "../../shared/room_visual_effects_trails.inc.c"

void func_shelter_b1_north_maintenance_walkway_80181940(Task* task)
{
#include "../../shared/room_visual_effects_trail_task.inc.c"
}

#include "../../shared/room_visual_effects_sparks.inc.c"

void func_shelter_b1_north_maintenance_walkway_80182228(Task* task)
{
    RoomFx_SparkBurstTask(task);
}

#include "../../shared/room_visual_effects_glow.inc.c"
#include "../../shared/room_visual_effects_flying_tasks.inc.c"

void func_shelter_b1_north_maintenance_walkway_80182E70(Task* arg0)
{
    RoomFx_GlowDiscTask(arg0);
}

void func_shelter_b1_north_maintenance_walkway_801833C8(Task* task)
{
    _roomVisualEffectsFlyingSparkTask(task);
}

#include "../../shared/room_visual_effects_burst.inc.c"

void func_shelter_b1_north_maintenance_walkway_80184028(Task* arg0)
{
    _roomVisualEffectsFlyingOrangeBurstTask(arg0);
}

#include "../../shared/room_visual_effects_burst_draw.inc.c"
