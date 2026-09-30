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
#include "gameplay/direction_input.h"
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

#include "rooms/rooms_shared_8017dcb8.h"

// Preserve the nonzero halfword after the three effect records.
// Its role is unresolved; it may be retained exporter padding.
typedef struct {
    RoomHaloShade entries[3];
    u16           retained;
} ShelterB1NorthMaintenanceWalkwayHaloStorage;
STATIC_ASSERT_SIZEOF(ShelterB1NorthMaintenanceWalkwayHaloStorage, 20);
static ShelterB1NorthMaintenanceWalkwayHaloStorage RoomFx_HaloShades;

/// Per-palette channel shifts for the halo, indexed by the palette the spawn
/// argument selects.

/// Offsets from the anchor of the two points the twin trail follows. The
/// second is also reached under its own name.

/// Per-colour channel shifts for the glowing disc, indexed by the spawn
/// argument.
static RoomHaloShade RoomFx_DiscShades[];

#include "../../shared/room_visual_effects.h"

#define ROOM_FX_HALO_STORAGE_INITIALIZER { { { 0, 1, 2 }, { 2, 1, 0 }, { 0, 2, 1 } }, 0x374F }
#define ROOM_FX_HALO_STORAGE_TYPE        ShelterB1NorthMaintenanceWalkwayHaloStorage
#define ROOM_FX_HALO_STORAGE_BOUND
#include "../../shared/room_visual_effects_data.inc.c"

static inline RoomHaloShade* RoomFx_GetHaloShades(void)
{
    return (RoomFx_HaloShades.entries);
}
#undef ROOM_FX_HALO_STORAGE_INITIALIZER
#undef ROOM_FX_HALO_STORAGE_TYPE
#undef ROOM_FX_HALO_STORAGE_BOUND

u8* D_shelter_b1_north_maintenance_walkway_80184B80[1] = {
    D_8010CAF8,
};

GpViewCountRec D_shelter_b1_north_maintenance_walkway_80184B84[1] = {
    { { .bytes = { 6, 0 } } },
};

GpWarpRec D_shelter_b1_north_maintenance_walkway_80184B88[2] = {
    { { .words = { 0, -2000, 0, 3600 } }, { 0, 0, 0, 0 }, { .words = { 0, -2000, 0, 3600 } }, { 0, 0, 0, 0 }, 0x540C0002, 0x540C0001, 0, 2, 0, 435 },
    { { .words = { 0, 1983, 0, -4420 } }, { 0, 0, 0, 0 }, { .words = { 0, 1983, 0, -4420 } }, { 0, 0, 0, 0 }, 0x540C0004, 0x540C0003, 0, 5, 0, 432 },
};

SVECTOR D_shelter_b1_north_maintenance_walkway_80184BF8[14] = {
#include "assets/shelter_b1_north_maintenance_walkway_collision_07980_normals.inc"
};

SVECTOR D_shelter_b1_north_maintenance_walkway_80184C68[38] = {
#include "assets/shelter_b1_north_maintenance_walkway_collision_07980_verts.inc"
};

GpGridFace D_shelter_b1_north_maintenance_walkway_80184D98[18] = {
#include "assets/shelter_b1_north_maintenance_walkway_collision_07980_faces.inc"
};

s16 D_shelter_b1_north_maintenance_walkway_80184E70[92] = {
#include "assets/shelter_b1_north_maintenance_walkway_collision_07980_cells.inc"
};

#define GRID_CELL(i) (&D_shelter_b1_north_maintenance_walkway_80184E70[i])
s16* D_shelter_b1_north_maintenance_walkway_80184F28[6] = {
#include "assets/shelter_b1_north_maintenance_walkway_collision_07980_table.inc"
};
#undef GRID_CELL

GpGridParams D_shelter_b1_north_maintenance_walkway_80184F40 = { NULL, D_shelter_b1_north_maintenance_walkway_80184BF8, D_shelter_b1_north_maintenance_walkway_80184C68, D_shelter_b1_north_maintenance_walkway_80184D98, D_shelter_b1_north_maintenance_walkway_80184F28, 2872, 4937, 2, 3, 4000, 18 };

GpViewRec D_shelter_b1_north_maintenance_walkway_80184F64[6] = {
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

GpSprtElem D_shelter_b1_north_maintenance_walkway_8018505C[38] = {
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

GpSprtRec D_shelter_b1_north_maintenance_walkway_801853AC[6] = {
    { { .empty = D_shelter_b1_north_maintenance_walkway_8018503C }, D_shelter_b1_north_maintenance_walkway_8018503C, NULL },
    { { .empty = D_shelter_b1_north_maintenance_walkway_8018504C }, D_shelter_b1_north_maintenance_walkway_8018504C, NULL },
    { { .elements = D_shelter_b1_north_maintenance_walkway_8018505C }, D_shelter_b1_north_maintenance_walkway_80185354, NULL },
    { { .empty = D_shelter_b1_north_maintenance_walkway_8018537C }, D_shelter_b1_north_maintenance_walkway_8018537C, NULL },
    { { .empty = D_shelter_b1_north_maintenance_walkway_8018538C }, D_shelter_b1_north_maintenance_walkway_8018538C, NULL },
    { { .empty = D_shelter_b1_north_maintenance_walkway_8018539C }, D_shelter_b1_north_maintenance_walkway_8018539C, NULL },
};

GpPointLight D_shelter_b1_north_maintenance_walkway_801853F4[5] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -1863, -221, 4731 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2758, 2836, 2918 }, { 0, 0 } }, 948, 2672 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 68, -223, 4315 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2698, 2860, 2900 }, { 0, 0 } }, 1359, 3223 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 2002, -504, -2007 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2785, 2839, 2881 }, { 0, 0 } }, 1799, 3522 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 1916, -2, 445 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2839, 2860, 2999 }, { 0, 0 } }, 1721, 3243 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 1988, -223, 2836 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2659, 2760, 2850 }, { 0, 0 } }, 1741, 3222 },
};

GpRoomCoordSet D_shelter_b1_north_maintenance_walkway_801855D4 = { 0, NULL, 5, D_shelter_b1_north_maintenance_walkway_801853F4, 0, NULL };

GpObj4C D_shelter_b1_north_maintenance_walkway_801855EC[6] = {
    { NULL, NULL, NULL, { 2080, -1584, -576, 0 }, { { -1376, -1904, 192, 0 }, { 1376, -1904, -192, 0 }, { -1376, 1904, 192, 0 }, { 1376, 1904, -192, 0 } }, { -569, 0, -4074, 0 }, { 0, 0, 4096, 0 }, 2346, 0, 5, 4, 1, 0 },
    { NULL, NULL, NULL, { 2048, -1600, -672, 0 }, { { 1408, -1904, -192, 0 }, { -1408, -1904, 192, 0 }, { 1408, 1904, -192, 0 }, { -1408, 1904, 192, 0 } }, { 554, 0, 4067, 0 }, { 0, 0, 4096, 0 }, 2374, 0, 4, 5, 1, 0 },
    { NULL, NULL, NULL, { 2240, -1504, 3200, 0 }, { { 1536, -1904, 256, 0 }, { -1536, -1904, -256, 0 }, { 1536, 1904, 256, 0 }, { -1536, 1904, -256, 0 } }, { -676, 0, 4053, 0 }, { 0, 0, 4096, 0 }, 2455, 0, 3, 4, 1, 0 },
    { NULL, NULL, NULL, { 2176, -1505, 3280, 0 }, { { -1536, -1904, -272, 0 }, { 1536, -1904, 272, 0 }, { -1536, 1904, -272, 0 }, { 1536, 1904, 272, 0 } }, { 714, 0, -4039, 0 }, { 0, 0, 4096, 0 }, 2455, 0, 4, 3, 1, 0 },
    { NULL, NULL, NULL, { -608, -1568, 4016, 0 }, { { 96, -1904, -1312, 0 }, { -96, -1904, 1312, 0 }, { 96, 1904, -1312, 0 }, { -96, 1904, 1312, 0 } }, { 4090, 0, 298, 0 }, { 0, 0, 4096, 0 }, 2304, 0, 3, 2, 1, 0 },
    { NULL, NULL, NULL, { -496, -1600, 3984, 0 }, { { -80, -1904, 1328, 0 }, { 80, -1904, -1328, 0 }, { -80, 1904, 1328, 0 }, { 80, 1904, -1328, 0 } }, { -4097, 0, -248, 0 }, { 0, 0, 4096, 0 }, 2318, 0, 2, 3, 129, 0 },
};

GpObj3A D_shelter_b1_north_maintenance_walkway_801857B4[1] = {
    { NULL, NULL, { -1280, -1104, 1776, 0 }, { { -1920, 2128, -848, 0 }, { 1920, 2128, 848, 0 }, { -1920, -2128, -848, 0 }, { 1920, -2128, 848, 0 } }, { -1662, 0, 3761, 0 }, { -87, 11 }, 129, 0 },
};

GpAreaTmdRec D_shelter_b1_north_maintenance_walkway_801857F0[3] = {
    { 21, 21, 0, 0, { 0, 0 }, D_80135C30 },
    { 24, 24, 1, 0, { 0, 0 }, D_8014E47C },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_shelter_b1_north_maintenance_walkway_80185814[2] = {
    { 3, 3, 0, 0, { 0, 0 }, D_80148110 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_shelter_b1_north_maintenance_walkway_8018582C[4] = {
    { 70, 70, 0, 0, { 0, 0 }, D_8013F5F0 },
    { 46, 46, 1, 0, { 0, 0 }, D_8014F698 },
    { 47, 47, 1, 0, { 0, 0 }, D_801502BC },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_shelter_b1_north_maintenance_walkway_8018585C[2] = {
    { 11, 11, 0, 0, { 0, 0 }, D_80147400 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_shelter_b1_north_maintenance_walkway_80185874[3] = {
    { 21, 21, 0, 0, { 0, 0 }, D_80135C30 },
    { 57, 57, 1, 0, { 0, 0 }, D_801611F8 },
    { 255, 0, 0, 0, { 0, 0 }, NULL },
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

GpAreaVariant D_shelter_b1_north_maintenance_walkway_80185A38[12] = {
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

GpObj4C D_shelter_b1_north_maintenance_walkway_80185A98[2] = {
    { NULL, NULL, NULL, { -2128, -48, 3344, 0 }, { { -720, 0, -400, 0 }, { 720, 0, -400, 0 }, { -720, 0, 400, 0 }, { 720, 0, 400, 0 } }, { 0, 4098, 0, 0 }, { 0, 0, 4096, 0 }, 822, 0, 14, 18, 2, 0 },
    { NULL, NULL, NULL, { 1984, -48, -4560, 0 }, { { -1024, 0, -448, 0 }, { 1024, 0, -448, 0 }, { -1024, 0, 448, 0 }, { 1024, 0, 448, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, 4096, 0 }, 1115, 0, 11, 35, 130, 0 },
};

s32 D_shelter_b1_north_maintenance_walkway_80185B30[3] = {
    0x1000005D,
    0x1000005F,
    0x1000005D,
};

GpRoomParamRec D_shelter_b1_north_maintenance_walkway_80185B3C[1] = {
    { 0, 0, 1, 0, NULL },
};

GpRoomParamRec D_shelter_b1_north_maintenance_walkway_80185B44[1] = {
    { 0, 0, 1, 0, D_shelter_b1_north_maintenance_walkway_80185B30 },
};

GpRoomParamRec* D_shelter_b1_north_maintenance_walkway_80185B4C[8] = {
    D_shelter_b1_north_maintenance_walkway_80185B3C,
    D_shelter_b1_north_maintenance_walkway_80185B44,
    D_shelter_b1_north_maintenance_walkway_80185B3C,
    D_shelter_b1_north_maintenance_walkway_80185B3C,
    D_shelter_b1_north_maintenance_walkway_80185B3C,
    D_shelter_b1_north_maintenance_walkway_80185B3C,
    D_shelter_b1_north_maintenance_walkway_80185B3C,
    D_shelter_b1_north_maintenance_walkway_80185B3C,
};

RoomFadeStorage D_shelter_b1_north_maintenance_walkway_80185B6C = { 0 };

RoomEventMsg D_shelter_b1_north_maintenance_walkway_80185B74 = { 0 };

s8 D_shelter_b1_north_maintenance_walkway_80185B7C[4] = {
    0,
    89,
    61,
    49,
};

RoomLatchedEvent D_shelter_b1_north_maintenance_walkway_80185B80;

/// Queues a grey gouraud glow spanning the projected points `arg0[0]` and
/// `arg0[1]`: a half-disc at each end, of radius `arg1` scaled by that end's
/// depth and turned by the angle `arg2`, joined by quads. The brightness
/// alternates between 0x20 and 0x28 on successive frames. Nothing is drawn when
/// the second point lies nearer than OTZ 0x11.
void func_shelter_b1_north_maintenance_walkway_8017DDE0(SVECTOR* arg0, s32 arg1, s32 arg2)
{
    u8*                head;
    RoomDraw11Scratch* block;
    POLY_G4*           prim;
    POLY_G4*           p;
    SVECTOR*           p1;
    s32                ang;
    s32                t;
    s32                t2;
    s32                t3;
    s32                rgb;
    s32                extent;
    s32                r0;
    s32                r1;
    s32                base;

    {
        void** scratch;
        u8*    tmp;

        scratch  = SCRATCH_STACK_CURSOR_SLOT;
        head     = *scratch;
        tmp      = head - 0x18;
        *scratch = tmp;
        p1       = arg0 + 1;
        block    = (RoomDraw11Scratch*)tmp;
    }

    gte_SetTransMatrix(&gGfxViewCoord.workm);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_ldv0(arg0);
    gte_rtps();
    gte_stsxy(&((RoomDraw11Scratch*)(head - 0x18))->sx0);
    gte_stszotz(&block->otz0);
    gte_ldv0(p1);
    gte_rtps();
    gte_stsxy(&((RoomDraw11Scratch*)(head - 0x18))->sx1);
    gte_stszotz(&((RoomDraw11Scratch*)(head - 0x18))->otz1);
    if (block->otz1 >= 0x11) {
        if (((RoomDraw11Scratch*)(head - 0x18))->otz0 < 0x10) {
            ((RoomDraw11Scratch*)(head - 0x18))->otz0 = 0x10;
        }
        extent    = (s16)arg1 * 64;
        r0        = extent / ((RoomDraw11Scratch*)(head - 0x18))->otz0;
        r1        = extent / block->otz1;
        ang       = 0;
        base      = (s16)arg2;
        rgb       = (((u8)gDisplayState.animFrame & 1) * 8) | 0x20;
        block->r0 = r0;
        block->r1 = r1;
        do {
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            p        = prim;
            p->r2    = rgb;
            p->g2    = rgb;
            prim->b2 = rgb;
            p->r3    = 0;
            p->g3    = 0;
            p->b3    = 0;
            p->x0    = block->sx0 + ((block->r0 * rsin(base + ang)) >> 12);
            p->y0    = block->sy0 + ((block->r0 * rcos(base + ang)) >> 12);
            t        = ang + 0x200;
            prim->x1 = block->sx0 + ((block->r0 * rsin(base + t)) >> 12);
            prim->y1 = block->sy0 + ((block->r0 * rcos(base + t)) >> 12);
            t2       = ang + 0x400;
            p->x2    = block->sx0;
            prim->y2 = block->sy0;
            prim->x3 = block->sx0 + ((block->r0 * rsin(base + t2)) >> 12);
            prim->y3 = block->sy0 + ((block->r0 * rcos(base + t2)) >> 12);
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz0 << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz0);

            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, rgb, rgb, rgb);
            setRGB3(prim, rgb, rgb, rgb);
            prim->x0 = block->sx0 + ((block->r0 * rsin(base + (ang * 2))) >> 12);
            prim->y0 = block->sy0 + ((block->r0 * rcos(base + (ang * 2))) >> 12);
            prim->x1 = block->sx1 + ((block->r1 * rsin(base + (ang * 2))) >> 12);
            prim->y1 = block->sy1 + ((block->r1 * rcos(base + (ang * 2))) >> 12);
            prim->x2 = block->sx0;
            prim->y2 = block->sy0;
            prim->x3 = block->sx1;
            prim->y3 = block->sy1;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz0 << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz0);

            t3             = ang - 0x1000;
            prim           = gGpuPrimCursor;
            t              = ang - 0x1000;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, rgb, rgb, rgb);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx1 + ((block->r1 * rsin(base - t3)) >> 12);
            prim->y0 = block->sy1 + ((block->r1 * rcos(base - t)) >> 12);
            t        = ang - 0xE00;
            prim->x1 = block->sx1 + ((block->r1 * rsin(base - t)) >> 12);
            prim->y1 = block->sy1 + ((block->r1 * rcos(base - t)) >> 12);
            t        = ang - 0xC00;
            prim->x2 = block->sx1;
            prim->y2 = block->sy1;
            t        = base - t;
            prim->x3 = block->sx1 + ((block->r1 * rsin(t)) >> 12);
            prim->y3 = block->sy1 + ((block->r1 * rcos(t)) >> 12);
            ang      = t2;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz1 << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz1);
        } while (ang < 0x800);
    }
    SCRATCH_POP_BYTES(0x18);
}

/// Projects `arg0` through `gGfxViewCoord.workm` and, when its OTZ is above 0x10,
/// queues four gouraud `POLY_G4` wedges forming a red disc around it, of radius
/// `arg1 * 64 / otz`. The centre's red level alternates between 0x20 and 0x28
/// on odd and even frames.
void func_shelter_b1_north_maintenance_walkway_8017E55C(SVECTOR* arg0, s16 arg1)
{
    RoomDraw25Scratch* block;
    POLY_G4*           prim;
    s32                ang;
    s32                t;
    s32                t2;
    s32                rgb;
    s32                radius;

    block = SCRATCH_PUSH(RoomDraw25Scratch);

    gte_SetTransMatrix(&gGfxViewCoord.workm);
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_ldv0(arg0);
    gte_rtps();
    gte_stsxy(&block->sx);
    gte_stszotz(&block->otz);
    if (block->otz >= 0x11) {
        radius        = (arg1 * 64) / block->otz;
        rgb           = (((u8)gDisplayState.animFrame & 1) * 8) | 0x20;
        ang           = 0;
        block->radius = radius;
        do {
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, rgb, 0, 0);
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
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        } while (ang < 0x1000);
    }
    SCRATCH_STACK_RELEASE_BLOCK(RoomDraw25Scratch);
}

#include "../../shared/room_visual_effects.inc.c"

void func_shelter_b1_north_maintenance_walkway_8017E8B8(Task* task)
{
    RoomFx_MoteTask(task);
}

#include "../../shared/room_visual_effects_halo.inc.c"

void func_shelter_b1_north_maintenance_walkway_8017F600(Task* arg0)
{
    RoomFx_HaloTask(arg0);
}

void func_shelter_b1_north_maintenance_walkway_8017F998(Task* arg0)
{
    RoomFx_OrangeBurstTask(arg0);
}

#include "../../shared/room_visual_effects_flash.inc.c"

void func_shelter_b1_north_maintenance_walkway_80180DA8(Task* arg0)
{
    RoomFx_SparkEmitterTask(arg0);
}

void func_shelter_b1_north_maintenance_walkway_80180EDC(Task* arg0)
{
    RoomFx_FlashTask(arg0);
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

void func_shelter_b1_north_maintenance_walkway_80182E70(Task* arg0)
{
    RoomFx_GlowDiscTask(arg0);
}

void func_shelter_b1_north_maintenance_walkway_801833C8(Task* task)
{
    RoomFx_FlyingSparkTask(task);
}

#include "../../shared/room_visual_effects_burst.inc.c"

void func_shelter_b1_north_maintenance_walkway_80184028(Task* arg0)
{
    RoomFx_OrangeBurst2Task(arg0);
}

#include "../../shared/room_visual_effects_burst_draw.inc.c"
