#include "rooms/acropolis_forked_road.h"

#include "common.h"

#include "acropolis_forked_road_private.h"

#include "gameplay/captions.h"
#include "gameplay/actor_presentation.h"
#include "gameplay/gameflag.h"
#include "gameplay/direction.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/items.h"
#include "gameplay/message.h"

#include "main/display.h"
#include "main/display_types.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/stream.h"
#include "main/stream_types.h"
#include "main/task.h"
#include "main/task_types.h"

/// The room's message table, installed on its entry task.
extern TaskMessageEntry D_acropolis_forked_road_80180F14[];

/// The room task's once-per-visit latch for the return ride, and the word that
/// follows it.
///
/// The return ride is the streamed scene that carries the player back along
/// the road when the session arrives here through warp 2. The room task polls
/// for that arrival every frame, so `spawned` keeps it from starting the scene
/// again. Nothing clears it; it is zero in the room's image, so it starts over
/// each time that image is loaded. The following word is zero and has no
/// recovered access; its role, and whether it belongs to the latch at all, is
/// unproven.
typedef struct {
    s32 spawned;    // Return-ride scene started this visit (0 not yet, 1 started)
    u8  field_4[4]; // Zero bytes with no accesses; role unproven
} _AcropolisForkedRoadReturnRideLatch;
STATIC_ASSERT_SIZEOF(_AcropolisForkedRoadReturnRideLatch, 8);

extern _AcropolisForkedRoadReturnRideLatch D_acropolis_forked_road_80180F3C;

static void _acropolisForkedRoadInitializeRoomTask(Task* task);
static void _acropolisForkedRoadStartReturnArrivalScene(Task* unusedTask);

/// State handlers of the room's own task.
static const TaskFuncTable3 D_acropolis_forked_road_8017D5C4 = {
    { _acropolisForkedRoadInitializeRoomTask, _acropolisForkedRoadStartReturnArrivalScene, taskKill }
};

static s32 _acropolisForkedRoadResolveTransitionMessage(Task* unusedTask, s32 messageId, const RoomEventMsg* request, RoomEventMsg* reply);
static s32 _acropolisForkedRoadRejectKeyItemUse(Task* unusedTask, s32 messageId, s32 itemId, s32 unusedArg);
static s32 _acropolisForkedRoadHandleCommandMessage(Task* unusedTask, s32 messageId, s32 commandId, s32 unusedArg);
static s32 _acropolisForkedRoadHandleActionMessage(Task* unusedTask, s32 messageId, const DirectionActionRequest* actionRequest, s32 unusedArg);

TaskMessageEntry D_acropolis_forked_road_80180F14[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _acropolisForkedRoadResolveTransitionMessage },
    { ROOM_MESSAGE_USE_KEY_ITEM, _acropolisForkedRoadRejectKeyItemUse },
    { ROOM_MESSAGE_COMMAND, _acropolisForkedRoadHandleCommandMessage },
    { DIRECTION_MESSAGE_ROOM_ACTION, _acropolisForkedRoadHandleActionMessage },
    { TASK_MESSAGE_TABLE_END, NULL },
};

_AcropolisForkedRoadReturnRideLatch D_acropolis_forked_road_80180F3C = { 0, { 0 } };

/// Resolves departures to the fountain or observatory and starts their pending scenes.
///
/// Borrows an eight-byte request and writable reply, which may alias. Copies
/// the request first. Returns 0 while a CAP or outbound movie scene handles
/// the departure, otherwise 1. Queries suppress scene starts and flag writes;
/// destination edits can differ between queries and execution. Requires the
/// room resources and live player; receiver and message ID are unused.
static s32 _acropolisForkedRoadResolveTransitionMessage(Task* unusedTask, s32 messageId, const RoomEventMsg* request, RoomEventMsg* reply)
{
    enum {
        ACROPOLIS_FORKED_ROAD_FOUNTAIN_CAP_SEQUENCE     = 1,
        ACROPOLIS_FORKED_ROAD_FIRST_PATH_CAP_VARIANT    = 3,
        ACROPOLIS_FORKED_ROAD_SECOND_LOCK_BIT           = 2,
        ACROPOLIS_FORKED_ROAD_FOUNTAIN_READY_PROGRESS   = 3,
        ACROPOLIS_FORKED_ROAD_ROUTE_MOVIE_READY         = 2,
        ACROPOLIS_FORKED_ROAD_ROUTE_MOVIE_STARTED       = 3,
        ACROPOLIS_FORKED_ROAD_DESTINATION_ROOM_UNLOCKED = 2,
        ACROPOLIS_FORKED_ROAD_OBSERVATORY_NORMAL_WARP   = 2,
        ACROPOLIS_FORKED_ROAD_OUTBOUND_MOVIE_VIEW       = 7,
        ACROPOLIS_FORKED_ROAD_TASK_OUTBOUND_MOVIE       = 0,
        ACROPOLIS_FORKED_ROAD_MAP_MARKER_SEEN           = 2,
        ACROPOLIS_FORKED_ROAD_ROUTE_CAP_COMMAND         = 2
    };

    *reply = *request;
    if (request->areaId == GAME_AREA_ACROPOLIS_FOUNTAIN) {
        if ((gameFlagGetNibble(GAME_FLAG_SECURITY_ROOM_LOCKS_RELEASED) & ACROPOLIS_FORKED_ROAD_SECOND_LOCK_BIT) && (request->queryOnly == ROOM_EVENT_EXECUTE)) {
            reply->room = ACROPOLIS_FORKED_ROAD_DESTINATION_ROOM_UNLOCKED;
        }
        if (request->areaId == GAME_AREA_ACROPOLIS_FOUNTAIN) {
            if (gameFlagGetNibble(GAME_FLAG_ACROPOLIS_PROGRESS) < ACROPOLIS_FORKED_ROAD_FOUNTAIN_READY_PROGRESS) {
                if (request->queryOnly == ROOM_EVENT_EXECUTE) {
                    gameFlagSetNibbleIfPresent(request->flagId, ACROPOLIS_FORKED_ROAD_MAP_MARKER_SEEN);
                    capStartSequenceSlot(ACROPOLIS_FORKED_ROAD_FOUNTAIN_CAP_SEQUENCE, CAP_PLAYBACK_DISPLAY_TRANSITION, 0);
                }
                return 0;
            }
            if (gameFlagGetNibble(GAME_FLAG_ACROPOLIS_PROGRESS) >= ACROPOLIS_FORKED_ROAD_FOUNTAIN_READY_PROGRESS) {
                if (gameFlagGetNibble(GAME_FLAG_FOUNTAIN_FORKED_ROAD_PATH_USED) == 0) {
                    if (request->queryOnly == ROOM_EVENT_EXECUTE) {
                        capStartSequenceSlot(ACROPOLIS_FORKED_ROAD_FOUNTAIN_CAP_SEQUENCE, CAP_PLAYBACK_DISPLAY_TRANSITION, ACROPOLIS_FORKED_ROAD_FIRST_PATH_CAP_VARIANT);
                        gameFlagSetNibble(GAME_FLAG_FOUNTAIN_FORKED_ROAD_PATH_USED, 1);
                    }
                }
                return 1;
            }
        }
    }
    if (request->areaId == GAME_AREA_ACROPOLIS_OBSERVATORY) {
        if (gameFlagGetNibble(GAME_FLAG_OBSERVATORY_ROUTE_PROGRESS) < ACROPOLIS_FORKED_ROAD_ROUTE_MOVIE_READY) {
            if (request->queryOnly == ROOM_EVENT_EXECUTE) {
                gameFlagSetNibbleIfPresent(request->flagId, ACROPOLIS_FORKED_ROAD_MAP_MARKER_SEEN);
                capRunCommandWithTransition(ACROPOLIS_FORKED_ROAD_ROUTE_CAP_COMMAND);
            }
            return 0;
        }
        if ((gDisplayState.debugMode < 0) || (D_8006AC30.startSector == 0)) {
            if (request->queryOnly != ROOM_EVENT_EXECUTE) {
                return 1;
            }
            reply->warp = ACROPOLIS_FORKED_ROAD_OBSERVATORY_NORMAL_WARP;
        } else if (gameFlagGetNibble(GAME_FLAG_OBSERVATORY_ROUTE_PROGRESS) == ACROPOLIS_FORKED_ROAD_ROUTE_MOVIE_READY) {
            if (request->queryOnly == ROOM_EVENT_EXECUTE) {
                // Select the movie view before starting the controller that owns the departure.
                gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = ACROPOLIS_FORKED_ROAD_OUTBOUND_MOVIE_VIEW;
                playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
                taskSpawnFromTable(D_acropolis_forked_road_80180F44, ACROPOLIS_FORKED_ROAD_TASK_OUTBOUND_MOVIE, 0, 0);
                gameFlagSetNibble(GAME_FLAG_OBSERVATORY_ROUTE_PROGRESS, ACROPOLIS_FORKED_ROAD_ROUTE_MOVIE_STARTED);
            }
            return 0;
        } else {
            reply->warp = ACROPOLIS_FORKED_ROAD_OBSERVATORY_NORMAL_WARP;
        }
        if (request->queryOnly != ROOM_EVENT_EXECUTE) {
            return 1;
        }
        if ((gameFlagGetNibble(GAME_FLAG_SECURITY_ROOM_LOCKS_RELEASED) & ACROPOLIS_FORKED_ROAD_SECOND_LOCK_BIT) == 0) {
            return 1;
        }
        if (gameFlagGetNibble(GAME_FLAG_ACROPOLIS_OBSERVATORY_EVENT_SEEN) != 0) {
            return 1;
        }
        reply->room = ACROPOLIS_FORKED_ROAD_DESTINATION_ROOM_UNLOCKED;
        return 1;
    }
    return 1;
}

/// Refuses key-item use without consuming the item or starting a room event.
///
/// All arguments are ignored. The reply selects the inventory's "No use now" notice.
static s32 _acropolisForkedRoadRejectKeyItemUse(Task* unusedTask, s32 messageId, s32 itemId, s32 unusedArg)
{
    return ROOM_KEY_ITEM_USE_REFUSED;
}

/// Selects CAP command 3 or 4 from placement 24's saved state for room command 3.
///
/// States 0 and 1 select CAP command 3; all others select 4. Other room commands
/// do nothing. The remaining arguments are unused; always returns zero.
static s32 _acropolisForkedRoadHandleCommandMessage(Task* unusedTask, s32 messageId, s32 commandId, s32 unusedArg)
{
    enum {
        ACROPOLIS_FORKED_ROAD_COMMAND_PLACEMENT_CAP = 3,
        ACROPOLIS_FORKED_ROAD_CAP_PLACEMENT_ID      = 24,
        ACROPOLIS_FORKED_ROAD_CAP_INITIAL_PLACEMENT = 3,
        ACROPOLIS_FORKED_ROAD_CAP_CHANGED_PLACEMENT = 4
    };
    s32 capCommand;

    if (commandId == ACROPOLIS_FORKED_ROAD_COMMAND_PLACEMENT_CAP) {
        if ((areaGetCurrentObjectState(ACROPOLIS_FORKED_ROAD_CAP_PLACEMENT_ID) == 0) || (areaGetCurrentObjectState(ACROPOLIS_FORKED_ROAD_CAP_PLACEMENT_ID) == 1)) {
            capCommand = ACROPOLIS_FORKED_ROAD_CAP_INITIAL_PLACEMENT;
        } else {
            capCommand = ACROPOLIS_FORKED_ROAD_CAP_CHANGED_PLACEMENT;
        }
        capRunCommandWithTransition(capCommand);
    }
    return 0;
}

/// Starts the once-only lock-release event for action 1 in placement variants 4 or 8.
///
/// Borrows the four-byte request through synchronous dispatch; no payload is
/// retained. Requires the second security lock released and the room's event
/// script loaded. Receiver, message ID and second word are unused; returns 1.
static s32 _acropolisForkedRoadHandleActionMessage(Task* unusedTask, s32 messageId, const DirectionActionRequest* actionRequest, s32 unusedArg)
{
    enum {
        ACROPOLIS_FORKED_ROAD_ACTION_LOCK_RELEASE_EVENT = 1,
        ACROPOLIS_FORKED_ROAD_EVENT_LOCK_BIT            = 2,
        ACROPOLIS_FORKED_ROAD_EVENT_VARIANT_4           = 4,
        ACROPOLIS_FORKED_ROAD_EVENT_VARIANT_8           = 8
    };
    u8 placementVariant;

    if (actionRequest->actionId == ACROPOLIS_FORKED_ROAD_ACTION_LOCK_RELEASE_EVENT && (gameFlagGetNibble(GAME_FLAG_SECURITY_ROOM_LOCKS_RELEASED) & ACROPOLIS_FORKED_ROAD_EVENT_LOCK_BIT)) {
        placementVariant = gGameSession->location.loc.variant;
        if (((placementVariant == ACROPOLIS_FORKED_ROAD_EVENT_VARIANT_4) || (placementVariant == ACROPOLIS_FORKED_ROAD_EVENT_VARIANT_8)) && (gameFlagGetNibble(GAME_FLAG_FORKED_ROAD_EVENT_SEEN) == 0)) {
            evsStartScript(D_acropolis_forked_road_801820B8, EVENT_SCRIPT_HUD_KEEP);
            gameFlagSetNibble(GAME_FLAG_FORKED_ROAD_EVENT_SEEN, 1);
        }
    }
    return 1;
}

/// Registers the forked road's room-message receiver and enters its active state.
///
/// Requires a live task in state 0 and loaded room resources. Borrows the message
/// table, publishes the task in `GAME_TASK_SLOT_ROOM` and advances to state 1.
static void _acropolisForkedRoadInitializeRoomTask(Task* task)
{
    task->msgTable = D_acropolis_forked_road_80180F14;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    task->state = task->state + 1;
}

/// Starts the return movie path once per overlay load when arriving through warp 2.
///
/// Sets the latch before spawning, so a failed spawn is not retried. Requires
/// the live session and return-scene resources; the room task is unused.
static void _acropolisForkedRoadStartReturnArrivalScene(Task* unusedTask)
{
    enum {
        ACROPOLIS_FORKED_ROAD_RETURN_ARRIVAL_WARP = 2,
        ACROPOLIS_FORKED_ROAD_TASK_RETURN_MOVIE   = 2
    };
    if ((D_acropolis_forked_road_80180F3C.spawned == 0) && (gGameSession->location.loc.warp == ACROPOLIS_FORKED_ROAD_RETURN_ARRIVAL_WARP)) {
        D_acropolis_forked_road_80180F3C.spawned = 1;
        taskSpawnFromTable(D_acropolis_forked_road_80180F44, ACROPOLIS_FORKED_ROAD_TASK_RETURN_MOVIE, 0, 0);
    }
}

void acropolisForkedRoadRoomTask(Task* task)
{
    TaskFuncTable3 stateHandlers = D_acropolis_forked_road_8017D5C4;
    stateHandlers.funcs[task->state](task);
}
