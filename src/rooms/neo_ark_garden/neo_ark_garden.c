#include "rooms/neo_ark_garden.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/rand.h>

#include "gte.h"
#include "types.h"

#include "neo_ark_garden_private.h"

#include "actors/actor_151000.h"
#include "actors/actor_01100.h"
#include "actors/actor_02400.h"
#include "actors/actor_03800.h"
#include "actors/actor_151000.h"

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
#include "main/sound_types.h"
#include "main/task_types.h"

#include "mapui/map_neo_ark.h"

#include "../../shared/room/room_variants.h"

#include "overlay.h"

s32     rcos(s32);
s32     rsin(s32);
MATRIX* TransposeMatrix(MATRIX*, MATRIX*);

/// Placement-entry keys and decimal file numbers for this room's resource recipes.
enum {
    NEO_ARK_GARDEN_RESOURCE_BLACK_BEETLE = 38,
    NEO_ARK_GARDEN_FILE_BLACK_BEETLE     = 38,
    NEO_ARK_GARDEN_RESOURCE_AYA_WALKER   = 132,
    NEO_ARK_GARDEN_FILE_AYA_WALKER       = 510,
    NEO_ARK_GARDEN_RESOURCE_MOSSBACK     = 49,
    NEO_ARK_GARDEN_FILE_MOSSBACK         = 49,
    NEO_ARK_GARDEN_RESOURCE_AMOEBA       = 24,
    NEO_ARK_GARDEN_FILE_AMOEBA           = 24,
};

/// Black Beetle resource recipes for placement variant 1.
///
/// Pairs with the map's variant-1 placements and ends at AREA_PLACEMENT_END.
/// Spawning borrows the room recipes, paired map placements and loaded actor
/// descriptors; retain those overlays through spawning and model-flag updates.
static AreaResource _gNeoArkGardenVariant1Resources[2] = {
    { .entryId = NEO_ARK_GARDEN_RESOURCE_BLACK_BEETLE, .fileNumber = NEO_ARK_GARDEN_FILE_BLACK_BEETLE, .fileGroupIndex = LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, .taskIndex = 0, .taskTable = &gActor103800BlackBeetleTaskDesc },
    { .entryId = AREA_PLACEMENT_END },
};

/// Aya Walker resource recipes for placement variant 2.
///
/// Pairs with the map's variant-2 placements and ends at AREA_PLACEMENT_END.
/// Spawning borrows the room recipes, paired map placements and loaded actor
/// descriptors; retain those overlays through spawning and model-flag updates.
static AreaResource _gNeoArkGardenVariant2Resources[2] = {
    { .entryId = NEO_ARK_GARDEN_RESOURCE_AYA_WALKER, .fileNumber = NEO_ARK_GARDEN_FILE_AYA_WALKER, .fileGroupIndex = LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, .taskIndex = 0, .taskTable = &gActor151000WalkerTaskDesc },
    { .entryId = AREA_PLACEMENT_END },
};

/// Black Beetle and Brute Mossback resource recipes for placement variant 3.
///
/// Six beetles and one Brute Mossback share this terminated recipe list.
/// Spawning borrows the room recipes, paired map placements and loaded actor
/// descriptors; retain those overlays through spawning and model-flag updates.
static AreaResource _gNeoArkGardenVariant3Resources[3] = {
    { .entryId = NEO_ARK_GARDEN_RESOURCE_BLACK_BEETLE, .fileNumber = NEO_ARK_GARDEN_FILE_BLACK_BEETLE, .fileGroupIndex = LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, .taskIndex = 0, .taskTable = &gActor103800BlackBeetleTaskDesc },
    { .entryId = NEO_ARK_GARDEN_RESOURCE_MOSSBACK, .fileNumber = NEO_ARK_GARDEN_FILE_MOSSBACK, .fileGroupIndex = AREA_RESOURCE_FILE_GROUP_BASE_20, .taskIndex = 0, .taskTable = gActor01100Tasks },
    { .entryId = AREA_PLACEMENT_END },
};

/// Amoeba resource recipes for placement variant 4.
///
/// Pairs with the map's variant-4 placements and ends at AREA_PLACEMENT_END.
/// Spawning borrows the room recipes, paired map placements and loaded actor
/// descriptors; retain those overlays through spawning and model-flag updates.
static AreaResource _gNeoArkGardenVariant4Resources[2] = {
    { .entryId = NEO_ARK_GARDEN_RESOURCE_AMOEBA, .fileNumber = NEO_ARK_GARDEN_FILE_AMOEBA, .fileGroupIndex = LOADING_AREA_FILE_GROUP_BASE_10_SELECTOR, .taskIndex = 0, .taskTable = gActor102400Tasks },
    { .entryId = AREA_PLACEMENT_END },
};

AreaVariant gNeoArkGardenAreaVariants[NEO_ARK_GARDEN_AREA_VARIANT_COUNT] = {
    [AREA_DEFAULT_VARIANT] = { .placements = gMapNeoArkGardenVariant1Placements, .resources = _gNeoArkGardenVariant1Resources },
    [2]                    = { .placements = gMapNeoArkGardenVariant2Placements, .resources = _gNeoArkGardenVariant2Resources },
    [3]                    = { .placements = gMapNeoArkGardenVariant3Placements, .resources = _gNeoArkGardenVariant3Resources },
    [4]                    = { .placements = gMapNeoArkGardenVariant4Placements, .resources = _gNeoArkGardenVariant4Resources },
};

/// Cue-2 sound requests in the loaded type-1 bank; cue 1 selects the next entry.
enum {
    NEO_ARK_GARDEN_SURFACE_4_WALK_CUE_BASE = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x0D,
    NEO_ARK_GARDEN_SURFACE_4_RUN_CUE_BASE  = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x0F
};

/// Footstep cue bases for surface class 4.
///
/// Walking and scripted jumps share one pair; running uses the other.
/// Cue 1 adds one to each base, and companion playback adds 100.
/// Requests borrow the loaded type-1 bank while the room is active.
static WorldCollisionFootstepSounds _gNeoArkGardenSurface4FootstepSounds = {
    .walk         = NEO_ARK_GARDEN_SURFACE_4_WALK_CUE_BASE,
    .run          = NEO_ARK_GARDEN_SURFACE_4_RUN_CUE_BASE,
    .scriptedJump = NEO_ARK_GARDEN_SURFACE_4_WALK_CUE_BASE,
};

/// Collision policy for surface classes 0..3 and 5..7: probes block, impacts are enabled and pushback applies.
///
/// No footstep cues; the loaded room overlay owns this record.
static WorldCollisionSurfaceProperties _gNeoArkGardenDefaultSurfaceProperties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = NULL,
};

/// Collision policy for surface class 4: probes block, impacts are enabled and pushback applies.
///
/// Borrows room-owned walking, running and scripted-jump cue bases while loaded.
static WorldCollisionSurfaceProperties _gNeoArkGardenSurface4Properties = {
    .field_0             = 0,
    .probePassThrough    = WORLD_COLLISION_SURFACE_BLOCK_PROBES,
    .weaponImpactEnabled = WORLD_COLLISION_SURFACE_ALLOW_WEAPON_IMPACTS,
    .suppressPushback    = WORLD_COLLISION_SURFACE_APPLY_PUSHBACK,
    .footstepSounds      = &_gNeoArkGardenSurface4FootstepSounds,
};

WorldCollisionSurfaceProperties* gNeoArkGardenSurfacePropertiesByClass[8] = {
    &_gNeoArkGardenDefaultSurfaceProperties,
    &_gNeoArkGardenDefaultSurfaceProperties,
    &_gNeoArkGardenDefaultSurfaceProperties,
    &_gNeoArkGardenDefaultSurfaceProperties,
    &_gNeoArkGardenSurface4Properties,
    &_gNeoArkGardenDefaultSurfaceProperties,
    &_gNeoArkGardenDefaultSurfaceProperties,
    &_gNeoArkGardenDefaultSurfaceProperties,
};

enum { NEO_ARK_GARDEN_AREA_MAP_MARK_SET = 1 };

/// Forest and bridge saved-area updates on the garden's first command-4 event.
///
/// Applies only while the first altar sequence is unsolved. Two updates plus
/// AREA_APPLY_END select layout 2, discard saved enemy poses and set map marks
/// in every mode. The synchronous apply call borrows these room-owned records.
static const AreaApplyRec _gNeoArkGardenCommand4AreaUpdates[3] SECTION(".data") = {
    { GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_FOREST_ZONE, 2, AREA_APPLY_MODE_ALWAYS | NEO_ARK_GARDEN_AREA_MAP_MARK_SET },
    { GAME_STAGE_SHELTER_NEO_ARK, GAME_AREA_NEO_ARK_BRIDGE, 2, AREA_APPLY_MODE_ALWAYS | NEO_ARK_GARDEN_AREA_MAP_MARK_SET },
    { AREA_APPLY_END, 0, 0, 0 },
};

static void _neoArkGardenInitializeRoom(Task* task);
static void _neoArkGardenRoomIdleState(Task* unusedTask);

#include "../../shared/effects/water.inc.c"

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
            areaApplySavedUpdates(_gNeoArkGardenCommand4AreaUpdates);
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

    task->msgTable = gNeoArkGardenRoomMessageTable;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    // Match arrival warp 3 and placement variant 2 as one halfword.
    if (*(u16*)&gGameSession->location.warp == NEO_ARK_GARDEN_ARRIVAL_KEY) {
        evsStartScriptWithSkip(gActor151000GardenArrivalScript, EVENT_SCRIPT_HUD_HIDE_RESTORE, gActor151000GardenArrivalSkipScript);
        gameFlagSetPackedByte(EVENT_FLAG_CURRENT_OBJECTIVE, NEO_ARK_GARDEN_ARRIVAL_OBJECTIVE);
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

/// Garden room callbacks: 0 register/conditional arrival scene, 1 idle, 2 release.
///
/// Dispatch copies all three callbacks and requires `Task::state` in 0..2.
/// No bounds check or terminator; keep the room loaded through task teardown.
static const TaskFuncTable3 _gNeoArkGardenRoomTaskStates = {
    { _neoArkGardenInitializeRoom, _neoArkGardenRoomIdleState, taskKill }
};

void neoArkGardenRoomTask(Task* task)
{
    TaskFuncTable3 handlers;

    handlers = _gNeoArkGardenRoomTaskStates;
    handlers.funcs[task->state](task);
}
