#include "rooms/neo_ark_garden.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/rand.h>

#include "gte.h"
#include "types.h"

#include "neo_ark_garden_private.h"

#include "actors/task_tables.h"

#include "gameplay/area.h"
#include "gameplay/area_flags.h"
#include "gameplay/area_transitions.h"
#include "gameplay/captions.h"
#include "gameplay/gameflag.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/scene_combat.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/gfxgte.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/task.h"
#include "main/task_types.h"

#include "mapui/map_neo_ark.h"

#include "../../shared/room_variants.h"

#include "overlay.h"

s32     rcos(s32);
s32     rsin(s32);
MATRIX* TransposeMatrix(MATRIX*, MATRIX*);

extern AreaApplyRec D_neo_ark_garden_80182BF8[];

extern EvsCommand D_actor_151000_801334EC[];
extern EvsCommand D_actor_151000_80133954[];

extern AreaResource D_neo_ark_garden_80182AE8[2];
extern AreaResource D_neo_ark_garden_80182B00[2];
extern AreaResource D_neo_ark_garden_80182B18[3];
extern AreaResource D_neo_ark_garden_80182B3C[2];

AreaResource D_neo_ark_garden_80182AE8[2] = {
    { 38, 38, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, D_actor_103800_80137D74 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_neo_ark_garden_80182B00[2] = {
    { 132, 510, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, &D_actor_151000_8013D2E0 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_neo_ark_garden_80182B18[3] = {
    { 38, 38, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, D_actor_103800_80137D74 },
    { 49, 49, AREA_RESOURCE_FILE_GROUP_BASE_20, 0, { 0, 0 }, D_actor_201100_8015F400 },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaResource D_neo_ark_garden_80182B3C[2] = {
    { 24, 24, LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, 0, { 0, 0 }, D_actor_102400_8013647C },
    { AREA_PLACEMENT_END, 0, 0, 0, { 0, 0 }, NULL },
};

AreaVariant D_neo_ark_garden_80182B54[13] = {
    { NULL, NULL },
    { D_map_neo_ark_8017B630, D_neo_ark_garden_80182AE8 },
    { D_map_neo_ark_8017B6E0, D_neo_ark_garden_80182B00 },
    { D_map_neo_ark_8017B700, D_neo_ark_garden_80182B18 },
    { D_map_neo_ark_8017B780, D_neo_ark_garden_80182B3C },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
};

WorldCollisionFootstepSounds D_neo_ark_garden_80182BBC = {
    0x1000000D,
    0x1000000F,
    0x1000000D,
};

WorldCollisionSurfaceProperties D_neo_ark_garden_80182BC8[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, NULL },
};

WorldCollisionSurfaceProperties D_neo_ark_garden_80182BD0[1] = {
    { 0, WORLD_COLLISION_SURFACE_BLOCK_PROBES, WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS, WORLD_COLLISION_SURFACE_APPLY_PUSHBACK, &D_neo_ark_garden_80182BBC },
};

WorldCollisionSurfaceProperties* D_neo_ark_garden_80182BD8[8] = {
    D_neo_ark_garden_80182BC8,
    D_neo_ark_garden_80182BC8,
    D_neo_ark_garden_80182BC8,
    D_neo_ark_garden_80182BC8,
    D_neo_ark_garden_80182BD0,
    D_neo_ark_garden_80182BC8,
    D_neo_ark_garden_80182BC8,
    D_neo_ark_garden_80182BC8,
};

AreaApplyRec D_neo_ark_garden_80182BF8[3] = {
    { 5, 11, 2, 1 },
    { 5, 27, 2, 1 },
    { 255, 0, 0, 0 },
};

static void _neoArkGardenInitializeRoom(Task* task);
static void _neoArkGardenRoomIdleState(Task* unusedTask);

#include "../../shared/water_refraction_task.inc.c"

#include "../../shared/water_distort_band_task.inc.c"

s32 neoArkGardenRejectKeyItemMessage(Task* unusedTask, s32 unusedMessageId, s32 unusedItemId, s32 unusedSecondArg)
{
    return ROOM_KEY_ITEM_USE_REFUSED;
}

s32 neoArkGardenResolveTransition(Task* unusedTask, s32 unusedMessageId, RoomEventMsg* request, RoomEventMsg* reply)
{
    enum {
        NEO_ARK_GARDEN_CAP_SUBSTATION_BLOCKED = 1,
        NEO_ARK_GARDEN_MAP_FLAG_VISIBLE       = 2,
    };

    *reply = *request;
    mapNeoArkResolveRoomVariant(request, reply);
    if (request->areaId != GAME_AREA_NEO_ARK_SUBSTATION) {
        return ROOM_VARIANT_TRANSITION_DIRECT;
    }
    if (gameFlagGetNibble(GAME_FLAG_NEO_ARK_ALTAR_SEQUENCE_1_SOLVED) != 0) {
        return ROOM_VARIANT_TRANSITION_DIRECT;
    }
    if (request->queryOnly != ROOM_EVENT_EXECUTE) {
        return ROOM_VARIANT_TRANSITION_REFUSED;
    }
    gameFlagSetNibbleIfPresent(request->flagId, NEO_ARK_GARDEN_MAP_FLAG_VISIBLE);
    capRunCommandWithTransition(NEO_ARK_GARDEN_CAP_SUBSTATION_BLOCKED);
    return ROOM_VARIANT_TRANSITION_REFUSED;
}

s32 neoArkGardenHandleCommand(Task* unusedTask, s32 unusedMessageId, s32 commandId, s32 unusedSecondArg)
{
    enum {
        NEO_ARK_GARDEN_COMMAND_AREA_UPDATE       = 4,
        NEO_ARK_GARDEN_COMMAND_5                 = 5,
        NEO_ARK_GARDEN_COMMAND_7                 = 7,
        NEO_ARK_GARDEN_CAP_AREA_UPDATE_ALTERNATE = 6,
        NEO_ARK_GARDEN_CAP_7_NURSERY_PROGRESS    = 9,
        NEO_ARK_GARDEN_CAP_5_NURSERY_PROGRESS    = 10,
        NEO_ARK_GARDEN_AREA_UPDATES_APPLIED      = 1,
    };

    if (commandId == NEO_ARK_GARDEN_COMMAND_AREA_UPDATE) {
        capRunCommandWithTransition(gameFlagGetNibble(GAME_FLAG_141) != 0 ? NEO_ARK_GARDEN_CAP_AREA_UPDATE_ALTERNATE : NEO_ARK_GARDEN_COMMAND_AREA_UPDATE);
        if ((gameFlagGetNibble(GAME_FLAG_NEO_ARK_GARDEN_0FA) == 0) && (gameFlagGetNibble(GAME_FLAG_NEO_ARK_ALTAR_SEQUENCE_1_SOLVED) == 0)) {
            gameFlagSetNibble(GAME_FLAG_NEO_ARK_GARDEN_0FA, NEO_ARK_GARDEN_AREA_UPDATES_APPLIED);
            areaApplySavedUpdates(D_neo_ark_garden_80182BF8);
        }
    }
    if (commandId == NEO_ARK_GARDEN_COMMAND_7) {
        capSpawnEventIfIdle(gameFlagGetNibble(GAME_FLAG_B6_NURSERY_PROGRESS) != 0 ? NEO_ARK_GARDEN_CAP_7_NURSERY_PROGRESS : NEO_ARK_GARDEN_COMMAND_7, CAP_EVENT_NO_FLAGS);
    }
    if (commandId == NEO_ARK_GARDEN_COMMAND_5) {
        capSpawnEventIfIdle(gameFlagGetNibble(GAME_FLAG_B6_NURSERY_PROGRESS) != 0 ? NEO_ARK_GARDEN_CAP_5_NURSERY_PROGRESS : NEO_ARK_GARDEN_COMMAND_5, CAP_EVENT_NO_FLAGS);
    }
    return 0;
}

s32 neoArkGardenIgnoreActionMessage(Task* unusedTask, s32 unusedMessageId, const DirectionActionRequest* unusedRequest, s32 unusedSecondArg)
{
    return 0;
}

/// Installs the garden receiver and starts the warp-3, variant-2 arrival scene.
///
/// The normal and skip scripts restore player control and the garden view.
/// The packed objective selector is advanced when the scene starts. The room
/// and the actor package supplying both borrowed scripts must remain loaded.
static void _neoArkGardenInitializeRoom(Task* task)
{
    enum {
        NEO_ARK_GARDEN_ARRIVAL_KEY       = (2 << 8) | 3, // Variant in the high byte, warp in the low byte.
        NEO_ARK_GARDEN_ARRIVAL_OBJECTIVE = 0x34,
    };

    task->msgTable = D_neo_ark_garden_801813B0;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    // Match arrival warp 3 and placement variant 2 as one halfword.
    if (*(u16*)&gGameSession->location.loc.warp == NEO_ARK_GARDEN_ARRIVAL_KEY) {
        evsStartScriptWithSkip(D_actor_151000_801334EC, EVENT_SCRIPT_HUD_HIDE_RESTORE, D_actor_151000_80133954);
        gameFlagSetPackedByte(GAME_FLAG_CURRENT_OBJECTIVE, NEO_ARK_GARDEN_ARRIVAL_OBJECTIVE);
    }
    task->state = task->state + 1;
}

/// Keeps the garden room task available for messages after initialization.
///
/// State 1 leaves `unusedTask` unchanged and performs no per-frame work.
static void _neoArkGardenRoomIdleState(Task* unusedTask)
{
    // Preserve the otherwise unused 16-byte stack reservation in this state.
    char stackFrame[0x10];
}

/// State handlers of the room's entry task: set-up, idle, then kill.
static const TaskFuncTable3 D_neo_ark_garden_8017D614 = {
    { _neoArkGardenInitializeRoom, _neoArkGardenRoomIdleState, taskKill }
};

void neoArkGardenRoomTask(Task* task)
{
    TaskFuncTable3 handlers;

    handlers = D_neo_ark_garden_8017D614;
    handlers.funcs[task->state](task);
}
