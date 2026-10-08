#include "common.h"

/* GCC orders BSS by first declaration; keep this prologue before the API headers. */
/// Volume last asked of the warehouse's ambient track, or 0 when none is
/// playing. Written by `dryfieldWarehouseAmbienceTask` and cleared by state 0
/// of the same task.
s32 D_dryfield_warehouse_801821B8;

#include "rooms/dryfield_warehouse.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "dryfield_warehouse_private.h"

#include "gameplay/area.h"
#include "gameplay/captions.h"
#include "gameplay/gameflag.h"
#include "gameplay/collision.h"
#include "gameplay/light.h"
#include "gameplay/message.h"
#include "gameplay/items.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/world_collision.h"

#include "main/coord.h"
#include "main/gameflag.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"

/// Cutscene task spawned by state 0, polled by `taskPollKill` in state 1 and
/// killed along with its parent in state 2.
extern Task* D_dryfield_warehouse_801821B4;

SpriteBatch D_dryfield_warehouse_801815F8[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_warehouse_80181608[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_warehouse_80181618[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteBatch D_dryfield_warehouse_80181628[2] = {
    { 0, 0, 0, 0, { 0, 0 } },
    { SPRITE_BATCH_END, 0, 0, 0, { 0, 0 } },
};

SpriteView D_dryfield_warehouse_80181638[9] = {
    { { .empty = D_dryfield_warehouse_801811A0 }, D_dryfield_warehouse_801811A0, NULL },
    { { .elements = D_dryfield_warehouse_801811B0 }, D_dryfield_warehouse_80181354, NULL },
    { { .elements = D_dryfield_warehouse_80181384 }, D_dryfield_warehouse_80181578, NULL },
    { { .elements = D_dryfield_warehouse_801815A8 }, D_dryfield_warehouse_801815D0, NULL },
    { { .empty = D_dryfield_warehouse_801815E8 }, D_dryfield_warehouse_801815E8, NULL },
    { { .elements = D_dryfield_warehouse_80181384 }, D_dryfield_warehouse_80181578, NULL },
    { { .elements = D_dryfield_warehouse_801815A8 }, D_dryfield_warehouse_801815D0, NULL },
    { { .empty = D_dryfield_warehouse_80181618 }, D_dryfield_warehouse_80181618, NULL },
    { { .empty = D_dryfield_warehouse_80181628 }, D_dryfield_warehouse_80181628, NULL },
};

WorldCollisionTrigger D_dryfield_warehouse_801816A4[4] = {
    { NULL, NULL, NULL, { 2927, -1584, -2065, 0 }, { { -487, 1744, 1952, 0 }, { 487, 1744, -1951, 0 }, { -487, -1744, 1952, 0 }, { 487, -1744, -1951, 0 } }, { 3985, 0, 994, 0 }, { 0, 0, 4096, 0 }, 2660, 0, 3, 2, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 3072, -960, -2114, 0 }, { { 487, 1984, -1951, 0 }, { -487, 1984, 1952, 0 }, { 487, -1984, -1951, 0 }, { -487, -1984, 1952, 0 } }, { -3980, 0, -994, 0 }, { 0, 0, 4096, 0 }, 2816, 0, 2, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 4703, -1104, -1858, 0 }, { { 1097, 2128, -1688, 0 }, { -1133, 2128, 1658, 0 }, { 1097, -2128, -1688, 0 }, { -1133, -2128, 1658, 0 } }, { -3424, 0, -2283, 0 }, { 0, 0, 4096, 0 }, 2918, 0, 3, 4, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY, 0 },
    { NULL, NULL, NULL, { 4607, -1168, -1922, 0 }, { { -1133, 2192, 1655, 0 }, { 1097, 2192, -1692, 0 }, { -1133, -2192, 1655, 0 }, { 1097, -2192, -1692, 0 } }, { 3424, 0, 2281, 0 }, { 0, 0, 4096, 0 }, 2974, 0, 4, 3, WORLD_COLLISION_TRIGGER_VIEW_BOUNDARY | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionTrigger D_dryfield_warehouse_801817D4[13] = {
    { NULL, NULL, NULL, { 2303, -63, -3664, 0 }, { { -543, 0, -208, 0 }, { 544, 0, -208, 0 }, { -543, 0, 208, 0 }, { 544, 0, 208, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, 4096, 0 }, 579, WORLD_COLLISION_TRIGGER_ACTION_WARP, 5, 19, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 5056, -61, -880, 0 }, { { 352, 0, -687, 0 }, { 352, 0, 688, 0 }, { -352, 0, -687, 0 }, { -352, 0, 688, 0 } }, { 0, 4100, 0, 0 }, { -4096, 0, 0, 0 }, 770, WORLD_COLLISION_TRIGGER_ACTION_WARP, 9, 34, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 5456, -64, -1824, 0 }, { { 528, 0, -703, 0 }, { 528, 0, 704, 0 }, { -528, 0, -703, 0 }, { -528, 0, 704, 0 } }, { 0, 4110, 0, 0 }, { -2897, 0, -2896, 0 }, 879, WORLD_COLLISION_TRIGGER_ACTION_CAP, 4, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 2992, -64, -416, 0 }, { { -1231, 0, -1008, 0 }, { 1232, 0, -1008, 0 }, { -1231, 0, 400, 0 }, { 1232, 0, 400, 0 } }, { 0, 4104, 0, 0 }, { 0, 0, 4096, 0 }, 1588, WORLD_COLLISION_TRIGGER_ACTION_CAP, 2, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 1456, -64, -1216, 0 }, { { -383, 0, -1168, 0 }, { 384, 0, -1168, 0 }, { -383, 0, 1168, 0 }, { 384, 0, 1168, 0 } }, { 0, 4100, 0, 0 }, { 4095, 0, 0, 0 }, 1227, WORLD_COLLISION_TRIGGER_ACTION_CAP, 1, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 736, -64, -2656, 0 }, { { -1119, 0, -240, 0 }, { 1120, 0, -240, 0 }, { -1119, 0, 240, 0 }, { 1120, 0, 240, 0 } }, { 0, 4096, 0, 0 }, { 0, 0, -4095, 0 }, 1144, WORLD_COLLISION_TRIGGER_ACTION_CAP, 1, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 656, -64, -3264, 0 }, { { -655, 0, -240, 0 }, { 656, 0, -240, 0 }, { -655, 0, 240, 0 }, { 656, 0, 240, 0 } }, { 0, 4105, 0, 0 }, { -201, 0, 4090, 0 }, 698, WORLD_COLLISION_TRIGGER_ACTION_CAP, 8, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 2944, -64, -3000, 0 }, { { -415, 0, -312, 0 }, { 416, 0, -312, 0 }, { -415, 0, 168, 0 }, { 416, 0, 456, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, 4096, 0 }, 617, WORLD_COLLISION_TRIGGER_ACTION_CAP, 7, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 1632, -64, -192, 0 }, { { -655, 0, -240, 0 }, { 656, 0, -240, 0 }, { -655, 0, 240, 0 }, { 656, 0, 240, 0 } }, { 0, 4105, 0, 0 }, { 201, 0, -4090, 0 }, 698, WORLD_COLLISION_TRIGGER_ACTION_CAP, 10, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4512, -64, -256, 0 }, { { -655, 0, -240, 0 }, { 656, 0, -240, 0 }, { -655, 0, 240, 0 }, { 656, 0, 240, 0 } }, { 0, 4105, 0, 0 }, { 201, 0, -4090, 0 }, 698, WORLD_COLLISION_TRIGGER_ACTION_CAP, 10, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 5216, -64, -1200, 0 }, { { 800, 0, -1119, 0 }, { 800, 0, 1120, 0 }, { -800, 0, -1119, 0 }, { -800, 0, 1120, 0 } }, { 0, 4100, 0, 0 }, { -4096, 0, 0, 0 }, 1372, WORLD_COLLISION_TRIGGER_ACTION_ROOM, WORLD_COLLISION_TRIGGER_ROOM_EVENT_ID, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 5888, -64, -1664, 0 }, { { 320, 0, -1119, 0 }, { 320, 0, 1664, 0 }, { -1472, 0, -1119, 0 }, { -1472, 0, 1664, 0 } }, { 0, 4104, 0, 0 }, { 201, 0, -4091, 0 }, 2217, WORLD_COLLISION_TRIGGER_ACTION_ROOM, WORLD_COLLISION_TRIGGER_ROOM_EVENT_ID, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 5344, -64, -1904, 0 }, { { 896, 0, -911, 0 }, { 896, 0, 912, 0 }, { -896, 0, -911, 0 }, { -896, 0, 912, 0 } }, { 0, 4095, 0, 0 }, { 201, 0, -4091, 0 }, 1273, WORLD_COLLISION_TRIGGER_ACTION_ROOM, WORLD_COLLISION_TRIGGER_ROOM_EVENT_ID, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCollisionTrigger D_dryfield_warehouse_80181BB0[10] = {
    { NULL, NULL, NULL, { 2303, -48, -3664, 0 }, { { -543, 0, -208, 0 }, { 544, 0, -208, 0 }, { -543, 0, 208, 0 }, { 544, 0, 208, 0 } }, { 0, 4095, 0, 0 }, { 0, 0, 4096, 0 }, 579, WORLD_COLLISION_TRIGGER_ACTION_WARP, 5, 19, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 5728, -48, -864, 0 }, { { 352, 0, -671, 0 }, { 352, 0, 672, 0 }, { -352, 0, -671, 0 }, { -352, 0, 672, 0 } }, { 0, 4102, 0, 0 }, { -4096, 0, 0, 0 }, 757, WORLD_COLLISION_TRIGGER_ACTION_WARP, 9, 34, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 5120, -64, -2256, 0 }, { { 352, 0, -655, 0 }, { 352, 0, 656, 0 }, { -352, 0, -655, 0 }, { -352, 0, 656, 0 } }, { 0, 4102, 0, 0 }, { -4096, 0, 0, 0 }, 743, WORLD_COLLISION_TRIGGER_ACTION_CAP, 9, 0, WORLD_COLLISION_TRIGGER_QUAD, 0 },
    { NULL, NULL, NULL, { 2992, -64, -416, 0 }, { { -1231, 0, -1008, 0 }, { 1232, 0, -1008, 0 }, { -1231, 0, 400, 0 }, { 1232, 0, 400, 0 } }, { 0, 4104, 0, 0 }, { 0, 0, 4096, 0 }, 1588, WORLD_COLLISION_TRIGGER_ACTION_CAP, 2, 0, WORLD_COLLISION_TRIGGER_NEAR_OR_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 1456, -64, -1216, 0 }, { { -383, 0, -1168, 0 }, { 384, 0, -1168, 0 }, { -383, 0, 1168, 0 }, { 384, 0, 1168, 0 } }, { 0, 4100, 0, 0 }, { 4095, 0, 0, 0 }, 1227, WORLD_COLLISION_TRIGGER_ACTION_CAP, 1, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 736, -64, -2656, 0 }, { { -1119, 0, -240, 0 }, { 1120, 0, -240, 0 }, { -1119, 0, 240, 0 }, { 1120, 0, 240, 0 } }, { 0, 4096, 0, 0 }, { 0, 0, -4095, 0 }, 1144, WORLD_COLLISION_TRIGGER_ACTION_CAP, 1, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 3120, -64, -2944, 0 }, { { -367, 0, -368, 0 }, { 368, 0, -368, 0 }, { -367, 0, 368, 0 }, { 368, 0, 368, 0 } }, { 0, 4097, 0, 0 }, { 0, 0, 4096, 0 }, 519, WORLD_COLLISION_TRIGGER_ACTION_CAP, 7, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 480, -64, -3168, 0 }, { { -847, 0, -208, 0 }, { 848, 0, -208, 0 }, { -847, 0, 208, 0 }, { 848, 0, 208, 0 } }, { 0, 4105, 0, 0 }, { 0, 0, 4096, 0 }, 872, WORLD_COLLISION_TRIGGER_ACTION_CAP, 8, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 1664, -64, -288, 0 }, { { -847, 0, -208, 0 }, { 848, 0, -208, 0 }, { -847, 0, 208, 0 }, { 848, 0, 208, 0 } }, { 0, 4105, 0, 0 }, { 0, 0, -4096, 0 }, 872, WORLD_COLLISION_TRIGGER_ACTION_CAP, 10, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD, 0 },
    { NULL, NULL, NULL, { 4736, -64, -336, 0 }, { { -847, 0, -256, 0 }, { 848, 0, -256, 0 }, { -847, 0, 256, 0 }, { 848, 0, 256, 0 } }, { 0, 4106, 0, 0 }, { 0, 0, -4096, 0 }, 884, WORLD_COLLISION_TRIGGER_ACTION_CAP, 10, 0, WORLD_COLLISION_TRIGGER_FACING_QUAD | WORLD_COLLISION_TRIGGER_LAST, 0 },
};

WorldCoordPointLight D_dryfield_warehouse_80181EA8[6] = {
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 2870, -1533, -81 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2048, 2048, 2048 }, { 0, 0 } }, 968, 3039 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 1060, -710, -1699 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 3280, 3280, 3280 }, { 0, 0 } }, 710, 1060 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 3317, -270, -901 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 1232, 1232, 1232 }, { 0, 0 } }, 561, 755 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 3416, -158, -3078 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2050, 2050, 2050 }, { 0, 0 } }, 1163, 1602 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 3067, -1528, -79 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 2050, 2052, 2052 }, { 0, 0 } }, 1300, 3031 },
    { { { .lighting = { GRAPHICS_COORD_DIRTY, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 3159, -1641, -3270 } }, { { { 4096, 0, 0 }, { 0, 4096, 0 }, { 0, 0, 4096 } }, { 0, 0, 0 } }, WORLD_COORDINATE_LIGHT_ALL_VIEWS, { 0, 0, 0, 0 }, 0, NULL } }, { 4096, 4096, 4096 }, { 0, 0 } }, 1941, 3832 },
};

WorldCoordRoomLights D_dryfield_warehouse_801820E8[1] = {
    { 0, NULL, ARRAY_SIZE(D_dryfield_warehouse_80181EA8), D_dryfield_warehouse_80181EA8, 0, NULL },
};

AreaVariant D_dryfield_warehouse_80182100[13] = { 0 };

WorldCollisionFootstepSounds D_dryfield_warehouse_80182168 = {
    0x10000045,
    0x10000047,
    0x10000045,
};

WorldCollisionSurfaceProperties D_dryfield_warehouse_80182174[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_dryfield_warehouse_8018217C[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_dryfield_warehouse_80182184[1] = {
    { 0, WORLD_COLLISION_SURFACE_PASS_PROBES, WORLD_COLLISION_SURFACE_IGNORE_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_dryfield_warehouse_8018218C[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_dryfield_warehouse_80182168 },
};

WorldCollisionSurfaceProperties* D_dryfield_warehouse_80182194[8] = {
    D_dryfield_warehouse_80182174,
    D_dryfield_warehouse_80182174,
    D_dryfield_warehouse_80182174,
    D_dryfield_warehouse_80182174,
    D_dryfield_warehouse_8018217C,
    D_dryfield_warehouse_80182184,
    D_dryfield_warehouse_8018218C,
    D_dryfield_warehouse_80182174,
};

Task* D_dryfield_warehouse_801821B4 = NULL;

Task* D_dryfield_warehouse_801821BC;

Task* D_dryfield_warehouse_801821C0;

s16 D_dryfield_warehouse_801821C4;

static void _dryfieldWarehouseInitializeRoomTask(Task* task);
static void _dryfieldWarehouseIdleRoomTask(Task* unusedTask);

void dryfieldWarehouseAmbienceTask(Task* task)
{
    enum {
        DRYFIELD_WAREHOUSE_AMBIENCE_INITIALIZE       = 0,
        DRYFIELD_WAREHOUSE_AMBIENCE_UPDATE           = 1,
        DRYFIELD_WAREHOUSE_AMBIENCE_SILENT           = 0,
        DRYFIELD_WAREHOUSE_AMBIENCE_LOW_VIEW         = 2,
        DRYFIELD_WAREHOUSE_AMBIENCE_MEDIUM_VIEW      = 3,
        DRYFIELD_WAREHOUSE_AMBIENCE_FULL_VIEW        = 4,
        DRYFIELD_WAREHOUSE_AMBIENCE_LOW_PERCENT      = 50,
        DRYFIELD_WAREHOUSE_AMBIENCE_MEDIUM_PERCENT   = 60,
        DRYFIELD_WAREHOUSE_AMBIENCE_FULL_PERCENT     = 100,
        DRYFIELD_WAREHOUSE_AMBIENCE_MAX_ATTENUATION  = 127,
        DRYFIELD_WAREHOUSE_AMBIENCE_STOP_AUDIO_TICKS = 30,
    };

    s32 targetVolumePercent;

    switch (task->state) {
        case DRYFIELD_WAREHOUSE_AMBIENCE_INITIALIZE:
            D_dryfield_warehouse_801821B8 = DRYFIELD_WAREHOUSE_AMBIENCE_SILENT;
            task->state                   = task->state + (DRYFIELD_WAREHOUSE_AMBIENCE_UPDATE - DRYFIELD_WAREHOUSE_AMBIENCE_INITIALIZE);
            return;
        case DRYFIELD_WAREHOUSE_AMBIENCE_UPDATE:
            break;
        default:
            return;
    }

    // Room events mute the ambience; idle views select a percentage gain.
    targetVolumePercent = DRYFIELD_WAREHOUSE_AMBIENCE_SILENT;
    if (gGameSession->eventState == 0) {
        switch (gGameSession->location.loc.view) {
            case DRYFIELD_WAREHOUSE_AMBIENCE_FULL_VIEW:
                targetVolumePercent = DRYFIELD_WAREHOUSE_AMBIENCE_FULL_PERCENT;
                break;
            case DRYFIELD_WAREHOUSE_AMBIENCE_MEDIUM_VIEW:
                targetVolumePercent = DRYFIELD_WAREHOUSE_AMBIENCE_MEDIUM_PERCENT;
                break;
            case DRYFIELD_WAREHOUSE_AMBIENCE_LOW_VIEW:
                targetVolumePercent = DRYFIELD_WAREHOUSE_AMBIENCE_LOW_PERCENT;
                break;
            default:
                targetVolumePercent = DRYFIELD_WAREHOUSE_AMBIENCE_SILENT;
                break;
        }
    }

    if (targetVolumePercent == D_dryfield_warehouse_801821B8) {
        return;
    }
    // Translate the percentage gain to the sound API's signed-byte attenuation.
    if (D_dryfield_warehouse_801821B8 == DRYFIELD_WAREHOUSE_AMBIENCE_SILENT) {
        sndEvtRequestScriptStart(SOUND_WAREHOUSE_AMBIENCE, 0, (s8)(((DRYFIELD_WAREHOUSE_AMBIENCE_FULL_PERCENT - targetVolumePercent) * DRYFIELD_WAREHOUSE_AMBIENCE_MAX_ATTENUATION) / DRYFIELD_WAREHOUSE_AMBIENCE_FULL_PERCENT));
    } else if (targetVolumePercent == DRYFIELD_WAREHOUSE_AMBIENCE_SILENT) {
        sndEvtRequestScriptStop(SOUND_WAREHOUSE_AMBIENCE, DRYFIELD_WAREHOUSE_AMBIENCE_STOP_AUDIO_TICKS);
    } else {
        sndEvtRequestScriptMix(SOUND_WAREHOUSE_AMBIENCE, 0, (s8)(((DRYFIELD_WAREHOUSE_AMBIENCE_FULL_PERCENT - targetVolumePercent) * DRYFIELD_WAREHOUSE_AMBIENCE_MAX_ATTENUATION) / DRYFIELD_WAREHOUSE_AMBIENCE_FULL_PERCENT));
    }
    D_dryfield_warehouse_801821B8 = targetVolumePercent;
}

/// Tests whether the live action list contains a hit warehouse event region.
///
/// Borrows the live, NULL-terminated trigger list without clearing hit latches
/// or retaining pointers. The complete control word must be the room-action
/// code: automatic and other flagged actions do not qualify. Tests the event
/// sentinel and hit latch only; it does not consume the Monkey Wrench itself.
static inline bool _dryfieldWarehouseHasHitEventTrigger(void)
{
    const WorldCollisionTrigger* trigger;

    trigger = Gp_PendingObj4C;
    while (trigger != NULL) {
        if (trigger->control == WORLD_COLLISION_TRIGGER_ACTION_ROOM && trigger->parameter0 == WORLD_COLLISION_TRIGGER_ROOM_EVENT_ID && trigger->hit != 0) {
            return true;
        }
        trigger = trigger->next;
    }
    return false;
}

s32 dryfieldWarehouseUseKeyItem(Task* task, s32 messageId, s32 itemId, s32 unusedSecondArg)
{
    enum { WAREHOUSE_EVENT_TASK = 0 };

    if (itemId == INVENTORY_COLLECTION_ID_MONKEY_WRENCH) {
        if (_dryfieldWarehouseHasHitEventTrigger() != 0) {
            gameFlagSetNibble(GAME_FLAG_WAREHOUSE_EVENT_SEEN, 1);
            gGameSession->eventState = 1;
            taskSpawnFromTableOnDefaultList(D_dryfield_warehouse_8017F56C, WAREHOUSE_EVENT_TASK, 0, 0);
            return ROOM_KEY_ITEM_USE_SHOW_USED_NOTICE;
        }
    }
    return ROOM_KEY_ITEM_USE_REFUSED;
}

s32 dryfieldWarehouseResolveRoomEvent(Task* task, s32 messageId, const RoomEventMsg* request, RoomEventMsg* reply)
{
    enum { WAREHOUSE_EXIT_BLOCKED_CAP    = 3,
           WAREHOUSE_EXIT_MAP_MARK       = 2,
           WAREHOUSE_AMBIENCE_STOP_TICKS = 15 };

    *reply = *request;
    if (request->areaId == GAME_AREA_DRYFIELD_DILAPIDATED_HOUSE) {
        if (gameFlagGetNibble(GAME_FLAG_WAREHOUSE_EVENT_SEEN) != 0) {
            return 1;
        }
        if (request->queryOnly == ROOM_EVENT_EXECUTE) {
            capRunCommandWithTransition(WAREHOUSE_EXIT_BLOCKED_CAP);
            gameFlagSetNibbleIfPresent(request->flagId, WAREHOUSE_EXIT_MAP_MARK);
        }
        return 0;
    }
    if (request->queryOnly == ROOM_EVENT_EXECUTE) {
        sndEvtRequestScriptStop(SOUND_WAREHOUSE_AMBIENCE, WAREHOUSE_AMBIENCE_STOP_TICKS);
    }
    return 1;
}

void dryfieldWarehouseEventTask(Task* task)
{
    enum { EVENT_START,
           EVENT_WAIT,
           EVENT_EXIT,
           CUTSCENE_TASK = 0 };
    s32 cutsceneResult;

    switch (task->state) {
        case EVENT_START:
            SetDispMask(0);
            D_dryfield_warehouse_801821B4 = taskSpawnFromTable(D_dryfield_warehouse_8017FB08, CUTSCENE_TASK, 0, 0);
            task->state                  += 1;
            return;
        case EVENT_WAIT:
            if (taskPollKill(D_dryfield_warehouse_801821B4, &cutsceneResult) != 0) {
                task->state += 1;
                return;
            }
            return;
        case EVENT_EXIT:
            taskKill(task);
            break;
    }
}

/// Registers the warehouse room receiver and starts its view-dependent ambience.
///
/// Installs the room message table, claims `GAME_TASK_SLOT_ROOM`, queues the
/// independent ambience task and advances to idle state 1. Keep this overlay's
/// message table, callbacks and sound resources loaded while those tasks run.
/// A failed ambience spawn does not prevent registration or state advancement.
static void _dryfieldWarehouseInitializeRoomTask(Task* task)
{
    enum { WAREHOUSE_AMBIENCE_TASK = 1 };

    task->msgTable = D_dryfield_warehouse_8017F554;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    taskSpawnFromTable(D_dryfield_warehouse_8017F56C, WAREHOUSE_AMBIENCE_TASK, 0, 0);
    task->state = task->state + 1;
}

/// Keeps the warehouse room task alive in state 1 to receive messages.
static void _dryfieldWarehouseIdleRoomTask(Task* unusedTask)
{
}

/// The three states of the room's main task, run by
/// `dryfieldWarehouseRoomTask`: set-up, the idle per-frame step and the
/// kill.
static const TaskFuncTable3 D_dryfield_warehouse_8017D5C4 = {
    { _dryfieldWarehouseInitializeRoomTask, _dryfieldWarehouseIdleRoomTask, taskKill },
};

void dryfieldWarehouseRoomTask(Task* task)
{
    TaskFuncTable3 stateHandlers;

    stateHandlers = D_dryfield_warehouse_8017D5C4;
    stateHandlers.funcs[task->state](task);
}
