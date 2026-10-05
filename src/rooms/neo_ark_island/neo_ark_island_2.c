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
// Exported instance: another image refers to this package's copy by name.
#define waterDriftTaskU16FixedCoord neoArkIslandWaterDriftTaskU16FixedCoord
#define waterRippleTaskFixedCoord   neoArkIslandWaterRippleTaskFixedCoord
#include "../../shared/water_effects.h"

/// Offsets from the parent coordinate of the two points whose trails
/// `func_neo_ark_island_80180600` records.
/// The second of those offsets, which the recording frames read by name.

TaskDesc D_neo_ark_island_80181B30 = { { { TASK_BODY_NONE, 192 } }, waterRefractionTask, { .value = 0 } };

TaskDesc D_neo_ark_island_80181B3C = { { { TASK_BODY_NONE, 192 } }, waterDistortBandTask, { .value = 0 } };

TaskMessageEntry D_neo_ark_island_80181B48[6] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, func_neo_ark_island_8017E968 },
    { 5105, func_neo_ark_island_8017E960 },
    { DIRECTION_MESSAGE_ROOM_ACTION, func_neo_ark_island_8017EA2C },
    { ROOM_MESSAGE_COMMAND, func_neo_ark_island_8017EA24 },
    { ROOM_MESSAGE_SOUND, func_neo_ark_island_8017EA34 },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc D_neo_ark_island_80181B78 = { { { TASK_BODY_NONE, 32 } }, func_neo_ark_island_8017E844, { .value = 0 } };

#include "../../shared/room_visual_effects_trail_data.inc.c"

WorldCollisionRoomResources D_neo_ark_island_80181B94[1] = {
    { D_neo_ark_island_801826C8, D_neo_ark_island_80183CC8, D_neo_ark_island_80183DF8, NULL },
};

WorldCoordRoomLighting D_neo_ark_island_80181BA4[1] = {
    { D_neo_ark_island_80183CB0, NULL },
};

u8* D_neo_ark_island_80181BAC[1] = {
    gViewIdentityMap,
};

ViewCount D_neo_ark_island_80181BB0[1] = { 5 };

DirectionWarpEntry D_neo_ark_island_80181BB4[2] = {
    { { { .word = 2048 }, 3100, 0, 6464 }, { 0, 0, 0, 0 }, { { .word = 2048 }, 3100, 0, 6464 }, { 0, 0, 0, 0 }, 0x550E0002, 0x550E0001, DIRECTION_WARP_SOUND_NONE, 2, DIRECTION_WARP_FLAG_NONE, DIRECTION_WARP_MAP_FLAG_NONE },
    { { { .word = ACTOR_SPAWN_YAW_KEEP_FACING }, 5300, 0, -1900 }, { 0, 0, 0, 0 }, { { .word = ACTOR_SPAWN_YAW_KEEP_FACING }, 5300, 0, -1900 }, { 0, 0, 0, 0 }, 0x550E0004, DIRECTION_WARP_SOUND_NONE, DIRECTION_WARP_SOUND_NONE, 4, DIRECTION_WARP_FLAG_FADE_DEPARTURE, DIRECTION_WARP_MAP_FLAG_NONE },
};

#include "../../shared/water_ripple_task_fixed_coord.inc.c"

#include "../../shared/water_splash.inc.c"

#include "../../shared/water_drift_task_u16_fixed_coord.inc.c"

#include "../../shared/water_spin_u16.inc.c"

#include "../../shared/water_tile_u16.inc.c"

void func_neo_ark_island_8017FB2C(Task* arg0)
{
    if (arg0->state == 0) {
        gRoomEffectFlashId       = EFFECT_NEO_ARK_ISLAND_FLASH;
        gRoomEffectTwinTrailId   = EFFECT_NEO_ARK_ISLAND_TWIN_TRAIL;
        gRoomEffectSparkBurstId  = EFFECT_NEO_ARK_ISLAND_SPARK_BURST;
        gRoomEffectWaterRippleId = EFFECT_NEO_ARK_ISLAND_WATER_RIPPLE;
        gRoomEffectWaterSprayId  = EFFECT_NEO_ARK_ISLAND_WATER_SPRAY;
        arg0->state              = 1;
    }
}

#include "../../shared/room_visual_effects.inc.c"

#include "../../shared/room_visual_effects_flash_task.inc.c"

void func_neo_ark_island_8017FB9C(Task* arg0)
{
    _roomVisualEffectsFlashTask(arg0);
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
