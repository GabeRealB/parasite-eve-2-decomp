#include "rooms/neo_ark_island.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "common.h"
#include "gte.h"

#include "neo_ark_island_private.h"

#include "gameplay/display.h"
#include "gameplay/actor_render.h"
#include "gameplay/direction.h"
#include "gameplay/direction_input.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/effects.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/view.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gamemain.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "rooms/room_common.h"
#include "../../shared/room_visual_effects.h"
/// Empty presence flag so `water_effects.h` declares the shared
/// `waterDrawSpinU16` and `waterDrawTileU16`. This file includes their bodies.
#define WATER_SHARED_U16_DRAWERS
#include "../../shared/water_effects.h"

/// Offsets from the parent coordinate of the two points whose trails
/// `func_neo_ark_island_80180600` records.
/// The second of those offsets, which the recording frames read by name.

TaskDesc D_neo_ark_island_80181B30 = { 0, 192, waterRefractionTask, { .model = NULL } };

TaskDesc D_neo_ark_island_80181B3C = { 0, 192, waterDistortBandTask, { .model = NULL } };

GpMsgEntry D_neo_ark_island_80181B48[6] = {
    { 5102, func_neo_ark_island_8017E968 },
    { 5105, func_neo_ark_island_8017E960 },
    { 5103, func_neo_ark_island_8017EA2C },
    { 5104, func_neo_ark_island_8017EA24 },
    { 5106, func_neo_ark_island_8017EA34 },
    { 0x7FFFFFFF, NULL },
};

TaskDesc D_neo_ark_island_80181B78 = { 0, 32, func_neo_ark_island_8017E844, { .model = NULL } };

#include "../../shared/room_visual_effects_trail_data.inc.c"

GpRoomObjRec D_neo_ark_island_80181B94[1] = {
    { D_neo_ark_island_801826C8, D_neo_ark_island_80183CC8, D_neo_ark_island_80183DF8, NULL },
};

GpRoomCoordRec D_neo_ark_island_80181BA4[1] = {
    { D_neo_ark_island_80183CB0, NULL },
};

u8* D_neo_ark_island_80181BAC[1] = {
    D_8010CAF8,
};

GpViewCountRec D_neo_ark_island_80181BB0[1] = {
    { { .bytes = { 5, 0 } } },
};

GpWarpRec D_neo_ark_island_80181BB4[2] = {
    { { .words = { 2048, 3100, 0, 6464 } }, { 0, 0, 0, 0 }, { .words = { 2048, 3100, 0, 6464 } }, { 0, 0, 0, 0 }, 0x550E0002, 0x550E0001, 0, 2, 0, 0 },
    { { .words = { 0x7FFE, 5300, 0, -1900 } }, { 0, 0, 0, 0 }, { .words = { 0x7FFE, 5300, 0, -1900 } }, { 0, 0, 0, 0 }, 0x550E0004, 0, 0, 4, 2, 0 },
};

#include "../../shared/water_ripple_task_fixed_coord.inc.c"

#include "../../shared/water_splash.inc.c"

#include "../../shared/water_drift_task_u16_fixed_coord.inc.c"

#include "../../shared/water_spin_u16.inc.c"

#include "../../shared/water_tile_u16.inc.c"

void func_neo_ark_island_8017FB2C(Task* arg0)
{
    if (arg0->state == 0) {
        D_80115758  = 0x601DB;
        D_8011572C  = 0x601F7;
        D_80115750  = 0x60213;
        D_8011574C  = 0x60178;
        D_80115738  = 0x60179;
        arg0->state = 1;
    }
}

#include "../../shared/room_visual_effects.inc.c"

#include "../../shared/room_visual_effects_flash_task.inc.c"

void func_neo_ark_island_8017FB9C(Task* arg0)
{
    RoomFx_FlashTask(arg0);
}

#include "../../shared/room_visual_effects_trails.inc.c"

void func_neo_ark_island_80180600(Task* task)
{
#include "../../shared/room_visual_effects_trail_task.inc.c"
}

#include "../../shared/room_visual_effects_sparks.inc.c"

void func_neo_ark_island_80180EE8(Task* task)
{
    RoomFx_SparkBurstTask(task);
}

#include "../../shared/room_visual_effects_glow.inc.c"
