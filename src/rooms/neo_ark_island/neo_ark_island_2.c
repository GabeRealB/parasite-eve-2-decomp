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
#include "main/sound_ids.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "rooms/room_common.h"
#include "../../shared/room/room_visual_effects.h"
/// Empty presence flag so `water_effects.h` declares the shared
/// `_waterDrawSpinU16` and `_waterDrawTileU16`. This file includes their bodies.
#define WATER_SHARED_U16_DRAWERS
/// Binds the shared water-spray definition to this package's public void (Task*)
/// callback. Define before including its fragment; the room header declares it.
#define WATER_SPRAY_TASK neoArkIslandWaterSprayTask
/// Binds the cached-coordinate ripple fragment to this package's public
/// void (Task*) callback. Define before its fragment; the room header declares
/// the export that gameplay's effect table imports. Takes no macro arguments.
#define WATER_RIPPLE_CACHED_COORD_TASK neoArkIslandWaterRippleTask
#include "../../shared/effects/water_effects.h"

TaskDesc gNeoArkIslandWaterRefractionTaskDesc = {
    .header   = { .fields = { .flags = TASK_BODY_NONE, .priority = TASK_PRIORITY_CONTROL } },
    .callback = waterRefractionTask,
    .data     = { .value = 0 },
};

/// Retained unused bodyless water-distortion recipe, priority 192.
///
/// One record with zero metadata and no terminator. Nothing spawns it; the
/// callback's drawing branches select only the submarine tunnel and gallery.
static TaskDesc _gNeoArkIslandUnusedWaterDistortionTaskDesc = {
    .header   = { .fields = { .flags = TASK_BODY_NONE, .priority = TASK_PRIORITY_CONTROL } },
    .callback = waterDistortBandTask,
    .data     = { .value = 0 },
};

TaskMessageEntry gNeoArkIslandRoomMessages[6] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, neoArkIslandResolveRoomTransition },
    { ROOM_MESSAGE_USE_KEY_ITEM, neoArkIslandRejectKeyItemMessage },
    { DIRECTION_MESSAGE_ROOM_ACTION, neoArkIslandIgnoreActionMessage },
    { ROOM_MESSAGE_COMMAND, neoArkIslandIgnoreCommandMessage },
    { ROOM_MESSAGE_SOUND, neoArkIslandSoundMessage },
    { TASK_MESSAGE_TABLE_END, NULL },
};

TaskDesc gNeoArkIslandGalleryDepartureTaskDesc = {
    .header   = { .fields = { .flags = TASK_BODY_NONE, .priority = TASK_PRIORITY_SCENE } },
    .callback = neoArkIslandGalleryDepartureTask,
    .data     = { .value = 0 },
};

#include "../../shared/room/room_visual_effects_trail_data.inc.c"

WorldCollisionRoomResources gNeoArkIslandRoomCollisionResources[1] = {
    {
        .grid                 = &gNeoArkIslandCollisionGrid,
        .viewBoundaryTriggers = gNeoArkIslandViewBoundaryTriggers,
        .actionTriggers       = gNeoArkIslandActionTriggers,
        .occluders            = NULL,
    },
};

WorldCoordRoomLighting gNeoArkIslandRoomLighting[1] = {
    { .lights = &gNeoArkIslandRoomLights, .ambientTable = NULL },
};

u8* gNeoArkIslandViewMaps[1] = {
    gViewIdentityMap,
};

ViewCount gNeoArkIslandViewCounts[1] = { NEO_ARK_ISLAND_VIEW_RESOURCE_COUNT };

DirectionWarpEntry gNeoArkIslandWarpEntries[2] = {
    {
        .player         = { .yaw = { .word = ACTOR_TRANSFORM_ANGLE_HALF_TURN }, .x = 3100, .y = 0, .z = 6464 },
        .unknown_10     = { 0 },
        .companion      = { .yaw = { .word = ACTOR_TRANSFORM_ANGLE_HALF_TURN }, .x = 3100, .y = 0, .z = 6464 },
        .unknown_24     = { 0 },
        .arrivalSound   = SOUND_AREA(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_ISLAND, 2),
        .departureSound = SOUND_AREA(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_ISLAND, 1),
        .blockedSound   = DIRECTION_WARP_SOUND_NONE,
        .initialView    = 2,
        .flags          = DIRECTION_WARP_FLAG_NONE,
        .mapFlagId      = DIRECTION_WARP_MAP_FLAG_NONE,
    },
    {
        .player         = { .yaw = { .word = ACTOR_SPAWN_YAW_KEEP_FACING }, .x = 5300, .y = 0, .z = -1900 },
        .unknown_10     = { 0 },
        .companion      = { .yaw = { .word = ACTOR_SPAWN_YAW_KEEP_FACING }, .x = 5300, .y = 0, .z = -1900 },
        .unknown_24     = { 0 },
        .arrivalSound   = SOUND_AREA(GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_ISLAND, 4),
        .departureSound = DIRECTION_WARP_SOUND_NONE,
        .blockedSound   = DIRECTION_WARP_SOUND_NONE,
        .initialView    = 4,
        .flags          = DIRECTION_WARP_FLAG_FADE_DEPARTURE,
        .mapFlagId      = DIRECTION_WARP_MAP_FLAG_NONE,
    },
};

#include "../../shared/effects/water_ripple_task_fixed_coord.inc.c"

#include "../../shared/effects/water_splash.inc.c"

#include "../../shared/effects/water_drift_task_u16_fixed_coord.inc.c"

#include "../../shared/effects/water_spin_u16.inc.c"

void neoArkIslandInstallRoomEffectIdsTask(Task* task)
{
    enum { EFFECT_IDS_PENDING,
           EFFECT_IDS_INSTALLED };

    if (task->state == EFFECT_IDS_PENDING) {
        gRoomEffectFlashId       = EFFECT_NEO_ARK_ISLAND_FLASH;
        gRoomEffectTwinTrailId   = EFFECT_NEO_ARK_ISLAND_TWIN_TRAIL;
        gRoomEffectSparkBurstId  = EFFECT_NEO_ARK_ISLAND_SPARK_BURST;
        gRoomEffectWaterRippleId = EFFECT_NEO_ARK_ISLAND_WATER_RIPPLE;
        gRoomEffectWaterSprayId  = EFFECT_NEO_ARK_ISLAND_WATER_SPRAY;
        task->state              = EFFECT_IDS_INSTALLED;
    }
}

#include "../../shared/room/room_visual_effects.inc.c"

#include "../../shared/room/room_visual_effects_flash_task.inc.c"

void neoArkIslandRoomVisualEffectsFlashTask(Task* task)
{
    _roomVisualEffectsFlashTask(task);
}

#include "../../shared/room/room_visual_effects_trails.inc.c"

void neoArkIslandRoomVisualEffectsTwinTrailTask(Task* task)
{
#include "../../shared/room/room_visual_effects_trail_task.inc.c"

    void neoArkIslandRoomVisualEffectsSparkBurstTask(Task * task)
    {
        _roomVisualEffectsSparkBurstTask(task);
    }

#include "../../shared/room/room_visual_effects_glow.inc.c"
