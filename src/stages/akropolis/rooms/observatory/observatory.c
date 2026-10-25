#include "stages/akropolis/rooms/observatory.h"

#include "config.h"
#include "../../event_flags.h"
#include "../../room_states.h"
#include "observatory_private.h"

#include "types.h"

#include "gameplay/direction.h"
#include "gameplay/message.h"
#include "gameplay/scene_combat.h"

#include "main/display.h"
#include "main/display_types.h"
#include "main/event_flags.h"
#include "main/gameflag.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/stream.h"
#include "main/stream_types.h"
#include "main/task.h"
#include "main/task_types.h"

/// Set when an arrival ride starts; zero whenever the room is loaded.
static s32 _gAkropolisObservatoryArrivalSceneStarted;

static s32 _akropolisObservatoryResolveRoomTransition(Task* unusedTask, s32 unusedMessageId, const RoomEventMsg* request, RoomEventMsg* reply);
static s32 _akropolisObservatoryRejectKeyItemUse(Task* unusedTask, s32 unusedMessageId, s32 unusedItemId, s32 unusedSecondArg);
static s32 _akropolisObservatoryHandleRoomAction(Task* unusedTask, s32 unusedMessageId, const DirectionActionRequest* request, s32 unusedSecondArg);

/// Messages of the room task.
static TaskMessageEntry _gAkropolisObservatoryRoomMessages[4] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _akropolisObservatoryResolveRoomTransition },
    { ROOM_MESSAGE_USE_KEY_ITEM, _akropolisObservatoryRejectKeyItemUse },
    { DIRECTION_MESSAGE_ROOM_ACTION, _akropolisObservatoryHandleRoomAction },
    { TASK_MESSAGE_TABLE_END, NULL },
};

static void _akropolisObservatoryInitializeRoomTask(Task* task);
static void _akropolisObservatoryStartArrivalScene(Task* unusedTask);

/// The doors: the forked road's plays the ride down the first time, unless there is no movie, and leads to its state
/// after the blue key; the promenade's plays the ride the first time the route reaches it, and leads to its state for
/// the bridge's progress.
static s32 _akropolisObservatoryResolveRoomTransition(Task* unusedTask, s32 unusedMessageId, const RoomEventMsg* request, RoomEventMsg* reply)
{
    s32 destinationRoom;

    *reply = *request;
    if (request->areaId == AKROPOLIS_ROOM_ID_FORKED_ROAD && request->queryOnly == ROOM_EVENT_EXECUTE) {
        if (gDisplayState.debugMode < 0 || gStreamInterFile.startSector == 0) {
            reply->warp = AKROPOLIS_OBSERVATORY_FORKED_ROAD_STAIRS_WARP;
        }
        if (gameFlagGetNibble(EVENT_FLAG_AKROPOLIS_OBSERVATORY_FORKED_ROAD_RIDE_SEEN) == 0) {
            gameFlagSetNibble(EVENT_FLAG_AKROPOLIS_OBSERVATORY_FORKED_ROAD_RIDE_SEEN, 1);
        } else {
            reply->warp = AKROPOLIS_OBSERVATORY_FORKED_ROAD_STAIRS_WARP;
        }
        if (request->areaId == AKROPOLIS_ROOM_ID_FORKED_ROAD) {
            if (gameFlagGetNibble(EVENT_FLAG_AKROPOLIS_SECURITY_ROOM_PANEL_KEYS) & AKROPOLIS_SECURITY_ROOM_PANEL_KEY_BLUE_INSERTED) {
                reply->roomState = AKROPOLIS_ROOM_STATE_FORKED_ROAD_ESCALATOR_OPERATIONAL;
            }
        }
    }
    if (request->areaId == AKROPOLIS_ROOM_ID_PROMENADE) {
        if (gDisplayState.debugMode < 0 || gStreamInterFile.startSector == 0) {
            if (request->queryOnly == ROOM_EVENT_EXECUTE) {
                reply->warp = AKROPOLIS_OBSERVATORY_PROMENADE_STAIRS_WARP;
            }
        }
        if (request->queryOnly == ROOM_EVENT_EXECUTE) {
            if (gameFlagGetNibble(EVENT_FLAG_AKROPOLIS_SANCTUARY_ROUTE_PROGRESS) == AKROPOLIS_SANCTUARY_ROUTE_OBSERVATORY_ENTERED) {
                gameFlagSetNibble(EVENT_FLAG_AKROPOLIS_SANCTUARY_ROUTE_PROGRESS, AKROPOLIS_SANCTUARY_ROUTE_PROMENADE_ENTERED);
            } else {
                reply->warp = AKROPOLIS_OBSERVATORY_PROMENADE_STAIRS_WARP;
            }
        }
        if (request->areaId == AKROPOLIS_ROOM_ID_PROMENADE && request->queryOnly == ROOM_EVENT_EXECUTE) {
            destinationRoom = gameFlagGetNibble(EVENT_FLAG_AKROPOLIS_BRIDGE_PROGRESS);
            if (destinationRoom == AKROPOLIS_BRIDGE_NOT_STARTED) {
                destinationRoom = AKROPOLIS_ROOM_STATE_PROMENADE_BEFORE_SANCTUARY;
            } else {
                destinationRoom = AKROPOLIS_ROOM_STATE_PROMENADE_AFTER_SANCTUARY;
            }
            reply->roomState = destinationRoom;
        }
    }
    return ROOM_EVENT_DEPART;
}

/// No key item can be used here.
static s32 _akropolisObservatoryRejectKeyItemUse(Task* unusedTask, s32 unusedMessageId, s32 unusedItemId, s32 unusedSecondArg)
{
    return ROOM_KEY_ITEM_USE_REFUSED;
}

/// Room action 1 starts the bat ambush once, in its state.
static s32 _akropolisObservatoryHandleRoomAction(Task* unusedTask, s32 unusedMessageId, const DirectionActionRequest* request, s32 unusedSecondArg)
{
    if ((request->actionId == AKROPOLIS_OBSERVATORY_ACTION_BAT_AMBUSH) &&
        (gGameSession->location.roomState == AKROPOLIS_ROOM_STATE_OBSERVATORY_BAT_AMBUSH) &&
        (gameFlagGetNibble(EVENT_FLAG_AKROPOLIS_OBSERVATORY_BAT_AMBUSH_SEEN) == 0)) {
        gameFlagSetNibble(EVENT_FLAG_AKROPOLIS_OBSERVATORY_BAT_AMBUSH_SEEN, 1);
        taskSpawnFromTable(&gAkropolisObservatoryScriptedBattleTaskDesc, 0, 0, 0);
    }
    return 0;
}

/// Installs the room's messages; in the bat ambush's state, before it has played, its waves are armed.
static void _akropolisObservatoryInitializeRoomTask(Task* task)
{
    enum { WAVES_ACTIVATED = 1 };

    task->msgTable = _gAkropolisObservatoryRoomMessages;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    if ((gGameSession->location.roomState == AKROPOLIS_ROOM_STATE_OBSERVATORY_BAT_AMBUSH) &&
        (gameFlagGetNibble(EVENT_FLAG_AKROPOLIS_OBSERVATORY_BAT_AMBUSH_SEEN) == 0)) {
        gSceneCombatState.actor03700Wave = WAVES_ACTIVATED;
    }
    task->state = task->state + 1;
}

/// Starts the arrival ride once, on arrival through either ride's warp.
static void _akropolisObservatoryStartArrivalScene(Task* unusedTask)
{
    if ((_gAkropolisObservatoryArrivalSceneStarted == 0) && (gGameSession->location.warp == AKROPOLIS_OBSERVATORY_PROMENADE_RIDE_ARRIVAL_WARP)) {
        _gAkropolisObservatoryArrivalSceneStarted = 1;
        taskSpawnFromTable(gAkropolisObservatoryArrivalTaskDescs, AKROPOLIS_OBSERVATORY_PROMENADE_RIDE_TASK_ENTRY, 0, 0);
    }
    if ((_gAkropolisObservatoryArrivalSceneStarted == 0) && (gGameSession->location.warp == AKROPOLIS_OBSERVATORY_FORKED_ROAD_RIDE_ARRIVAL_WARP)) {
        _gAkropolisObservatoryArrivalSceneStarted = 1;
        taskSpawnFromTable(gAkropolisObservatoryArrivalTaskDescs, AKROPOLIS_OBSERVATORY_FORKED_ROAD_RIDE_TASK_ENTRY, 0, 0);
    }
}

/// Room task states: initialize, start the arrival ride, release.
static const TaskFuncTable3 _gAkropolisObservatoryRoomTaskStates = {
    { _akropolisObservatoryInitializeRoomTask, _akropolisObservatoryStartArrivalScene, taskKill },
};

void akropolisObservatoryRoomTask(Task* task)
{
    TaskFuncTable3 stateHandlers = _gAkropolisObservatoryRoomTaskStates;
    stateHandlers.funcs[task->state](task);
}
