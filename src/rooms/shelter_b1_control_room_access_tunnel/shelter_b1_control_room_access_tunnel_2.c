#include "rooms/shelter_b1_control_room_access_tunnel.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "gte.h"
#include "types.h"

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
#include "gameplay/light.h"
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
#include "main/scratch.h"
#include "main/session.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "rooms/room.h"

#include "../../shared/room_visual_effects.h"

#include "../../shared/room_visual_effects_disc_data.inc.c"

u8* D_shelter_b1_control_room_access_tunnel_80181F00[1] = {
    D_8010CAF8,
};

GpViewCountRec D_shelter_b1_control_room_access_tunnel_80181F04[1] = {
    { { .bytes = { 3, 0 } } },
};

GpWarpRec D_shelter_b1_control_room_access_tunnel_80181F08[2] = {
    { { .words = { 3072, 5389, -25, 194 } }, { 0, 0, 0, 0 }, { .words = { 3072, 5389, -25, 194 } }, { 0, 0, 0, 0 }, 0x54190002, 0x54190001, 0, 2, 0, 0 },
    { { .words = { 1024, 831, -24, 119 } }, { 0, 0, 0, 0 }, { .words = { 1024, 831, -24, 119 } }, { 0, 0, 0, 0 }, 0x54190004, 0x54190003, 0, 3, 0, 0 },
};

SVECTOR D_shelter_b1_control_room_access_tunnel_80181F78[6] = {
#include "assets/shelter_b1_control_room_access_tunnel_collision_04AB0_normals.inc"
};

SVECTOR D_shelter_b1_control_room_access_tunnel_80181FA8[12] = {
#include "assets/shelter_b1_control_room_access_tunnel_collision_04AB0_verts.inc"
};

GpGridFace D_shelter_b1_control_room_access_tunnel_80182008[6] = {
#include "assets/shelter_b1_control_room_access_tunnel_collision_04AB0_faces.inc"
};

s16 D_shelter_b1_control_room_access_tunnel_80182050[12] = {
#include "assets/shelter_b1_control_room_access_tunnel_collision_04AB0_cells.inc"
};

#define GRID_CELL(i) (&D_shelter_b1_control_room_access_tunnel_80182050[i])
s16* D_shelter_b1_control_room_access_tunnel_80182068[2] = {
#include "assets/shelter_b1_control_room_access_tunnel_collision_04AB0_table.inc"
};
#undef GRID_CELL

GpGridParams D_shelter_b1_control_room_access_tunnel_80182070 = { NULL, D_shelter_b1_control_room_access_tunnel_80181F78, D_shelter_b1_control_room_access_tunnel_80181FA8, D_shelter_b1_control_room_access_tunnel_80182008, D_shelter_b1_control_room_access_tunnel_80182068, -106, 1242, 2, 1, 4000, 6 };

GpViewRec D_shelter_b1_control_room_access_tunnel_80182094[3] = {
    { { { { 4096, 0, 0 }, { 0, 0, -4096 }, { 0, 4096, 0 } }, { -3648, 8565, 0 } }, 235 },
    { { { { -16, 0, -4095 }, { -708, 4034, 2 }, { 4034, 708, -16 } }, { -837, 1954, -102 } }, 257 },
    { { { { 32, 0, 4095 }, { 347, 4081, -2 }, { -4081, 347, 32 } }, { -6111, 1435, -85 } }, 246 },
};

SpriteBatch D_shelter_b1_control_room_access_tunnel_80182100[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b1_control_room_access_tunnel_80182110[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_shelter_b1_control_room_access_tunnel_80182120[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

GpSprtRec D_shelter_b1_control_room_access_tunnel_80182130[3] = {
    { { .empty = D_shelter_b1_control_room_access_tunnel_80182100 }, D_shelter_b1_control_room_access_tunnel_80182100, NULL },
    { { .empty = D_shelter_b1_control_room_access_tunnel_80182110 }, D_shelter_b1_control_room_access_tunnel_80182110, NULL },
    { { .empty = D_shelter_b1_control_room_access_tunnel_80182120 }, D_shelter_b1_control_room_access_tunnel_80182120, NULL },
};

WorldCoordPointLight D_shelter_b1_control_room_access_tunnel_80182154[4] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 4469, -223, 260 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2498, 2539, 2560 }, { 0, 0 } }, 1500, 2000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 1886, -223, 190 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2498, 2539, 2560 }, { 0, 0 } }, 1500, 2000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { -1923, -223, 280 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2498, 2539, 2560 }, { 0, 0 } }, 1500, 2000 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 3158, -223, 433 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2498, 2539, 2560 }, { 0, 0 } }, 1500, 2000 },
};

GpRoomCoordSet D_shelter_b1_control_room_access_tunnel_801822D4 = { 0, NULL, 4, D_shelter_b1_control_room_access_tunnel_80182154, 0, NULL };

GpObj4C D_shelter_b1_control_room_access_tunnel_801822EC[2] = {
    { NULL, NULL, NULL, { 3454, -1167, 45, 0 }, { { -10, -1520, 2263, 0 }, { 11, -1520, -2263, 0 }, { -10, 1520, 2263, 0 }, { 11, 1520, -2263, 0 } }, { -4099, 0, -20, 0 }, { 0, 0, 4096, 0 }, 2721, 0, 3, 2, 1, 0 },
    { NULL, NULL, NULL, { 3311, -1152, 47, 0 }, { { 27, -1520, -2262, 0 }, { -26, -1520, 2263, 0 }, { 27, 1520, -2262, 0 }, { -26, 1520, 2263, 0 } }, { 4095, 0, 47, 0 }, { 0, 0, 4096, 0 }, 2721, 0, 2, 3, 129, 0 },
};

GpObj4C D_shelter_b1_control_room_access_tunnel_80182384[2] = {
    { NULL, NULL, NULL, { 5568, -48, 64, 0 }, { { -512, 0, -1024, 0 }, { 512, 0, -1024, 0 }, { -512, 0, 1024, 0 }, { 512, 0, 1024, 0 } }, { 0, 4096, 0, 0 }, { -4096, 0, 0, 0 }, 1144, 0, 15, 18, 2, 0 },
    { NULL, NULL, NULL, { 800, -48, 64, 0 }, { { -512, 0, -1024, 0 }, { 512, 0, -1024, 0 }, { -512, 0, 1024, 0 }, { 512, 0, 1024, 0 } }, { 0, 4096, 0, 0 }, { 4096, 0, 0, 0 }, 1144, 0, 18, 33, 130, 0 },
};

GpAreaTmdRec D_shelter_b1_control_room_access_tunnel_8018241C[3] = {
    { 21, 21, 0, 0, { 0, 0 }, D_80135C30 },
    { 24, 24, 1, 0, { 0, 0 }, D_8014E47C },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_shelter_b1_control_room_access_tunnel_80182440[3] = {
    { 21, 21, 0, 0, { 0, 0 }, D_80135C30 },
    { 11, 11, 1, 0, { 0, 0 }, D_8015F400 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_shelter_b1_control_room_access_tunnel_80182464[2] = {
    { 11, 11, 0, 0, { 0, 0 }, D_80147400 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_shelter_b1_control_room_access_tunnel_8018247C[3] = {
    { 21, 21, 0, 0, { 0, 0 }, D_80135C30 },
    { 20, 20, 1, 0, { 0, 0 }, D_8015FDF0 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

GpAreaTmdRec D_shelter_b1_control_room_access_tunnel_801824A0[1] = {
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaPlacement D_shelter_b1_control_room_access_tunnel_801824AC[5] = {
    { 21, 2, 0, 400, -2000, 800, 1024, 0, 0, 2, 1 },
    { 21, 2, 0, 400, -2000, -650, 1024, 0, 0, 2, 1 },
    { 24, 0, 0, 2650, 0, -350, 700, 0, 2, 4, 0 },
    { 24, 0, 0, 2250, 0, 650, 1300, 0, 2, 4, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaPlacement D_shelter_b1_control_room_access_tunnel_801824FC[4] = {
    { 21, 2, 0, 400, -2000, 800, 1024, 0, 0, 2, 1 },
    { 21, 2, 0, 400, -2000, -650, 1024, 0, 0, 2, 1 },
    { 11, 0, 0, 2600, 0, 0, 3072, 0, 2, 4, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaPlacement D_shelter_b1_control_room_access_tunnel_8018253C[2] = {
    { 11, 0, 0, 1500, 0, 140, 0, 0, 0, 2, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaPlacement D_shelter_b1_control_room_access_tunnel_8018255C[4] = {
    { 21, 3, 0, 400, -2000, 800, 1024, 0, 0, 2, 4 },
    { 21, 3, 0, 400, -2000, -650, 1024, 0, 0, 2, 4 },
    { 20, 4, 1, 1500, 0, 0, 1024, 0, 2, 4, 0 },
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

AreaPlacement D_shelter_b1_control_room_access_tunnel_8018259C[1] = {
    { AREA_PLACEMENT_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

GpAreaVariant D_shelter_b1_control_room_access_tunnel_801825AC[22] = {
    { NULL, NULL },
    { D_shelter_b1_control_room_access_tunnel_801824AC, D_shelter_b1_control_room_access_tunnel_8018241C },
    { D_shelter_b1_control_room_access_tunnel_801824FC, D_shelter_b1_control_room_access_tunnel_80182440 },
    { D_shelter_b1_control_room_access_tunnel_8018253C, D_shelter_b1_control_room_access_tunnel_80182464 },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { D_shelter_b1_control_room_access_tunnel_8018255C, D_shelter_b1_control_room_access_tunnel_8018247C },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { D_shelter_b1_control_room_access_tunnel_8018259C, D_shelter_b1_control_room_access_tunnel_801824A0 },
};

WorldCollisionFootstepSounds D_shelter_b1_control_room_access_tunnel_8018265C = {
    0x1000005D,
    0x1000005F,
    0x1000005D,
};

WorldCollisionSurfaceProperties D_shelter_b1_control_room_access_tunnel_80182668[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_shelter_b1_control_room_access_tunnel_80182670[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_shelter_b1_control_room_access_tunnel_8018265C },
};

WorldCollisionSurfaceProperties* D_shelter_b1_control_room_access_tunnel_80182678[8] = {
    D_shelter_b1_control_room_access_tunnel_80182668,
    D_shelter_b1_control_room_access_tunnel_80182670,
    D_shelter_b1_control_room_access_tunnel_80182668,
    D_shelter_b1_control_room_access_tunnel_80182668,
    D_shelter_b1_control_room_access_tunnel_80182668,
    D_shelter_b1_control_room_access_tunnel_80182668,
    D_shelter_b1_control_room_access_tunnel_80182668,
    D_shelter_b1_control_room_access_tunnel_80182668,
};

#include "../../shared/room_visual_effects_flying_tasks.inc.c"

void func_shelter_b1_control_room_access_tunnel_8018026C(Task* arg0)
{
    RoomFx_GlowDiscTask(arg0);
}

void func_shelter_b1_control_room_access_tunnel_801807C4(Task* task)
{
    RoomFx_FlyingSparkTask(task);
}

#include "../../shared/room_visual_effects_burst.inc.c"

void func_shelter_b1_control_room_access_tunnel_80181424(Task* arg0)
{
    RoomFx_OrangeBurst2Task(arg0);
}

#include "../../shared/room_visual_effects_burst_draw.inc.c"
