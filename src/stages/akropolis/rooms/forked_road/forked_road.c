#include "stages/akropolis/rooms/forked_road.h"

#include "config.h"
#include "../../event_flags.h"
#include "../../room_states.h"
#include "forked_road_private.h"

#include "common.h"

#include "gameplay/actor_presentation.h"
#include "gameplay/captions.h"
#include "gameplay/direction.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/gameflag.h"
#include "gameplay/items.h"
#include "gameplay/message.h"

#include "main/display.h"
#include "main/display_types.h"
#include "main/event_flags.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/stream.h"
#include "main/stream_types.h"
#include "main/task.h"
#include "main/task_types.h"

/// Whether the return ride has started this visit, and a zero word of unproven role after it.
typedef struct {
    s32 spawned;    // Return-ride scene started this visit (0 not yet, 1 started)
    u8  field_4[4]; // Zero bytes with no accesses; role unproven
} _ReturnRideLatch;
STATIC_ASSERT_SIZEOF(_ReturnRideLatch, 8);

static void _akropolisForkedRoadInitializeRoomTask(Task* task);
static void _akropolisForkedRoadStartReturnArrivalScene(Task* unusedTask);

/// Room task states: initialize, start the return ride on arrival through its warp, release.
static const TaskFuncTable3 _gAkropolisForkedRoadRoomTaskStates = {
    { _akropolisForkedRoadInitializeRoomTask, _akropolisForkedRoadStartReturnArrivalScene, taskKill }
};

static s32 _akropolisForkedRoadResolveRoomTransition(Task* unusedTask, s32 unusedMessageId, const RoomEventMsg* request, RoomEventMsg* reply);
static s32 _akropolisForkedRoadRejectKeyItemUse(Task* unusedTask, s32 unusedMessageId, s32 unusedItemId, s32 unusedSecondArg);
static s32 _akropolisForkedRoadHandleRoomCommand(Task* unusedTask, s32 unusedMessageId, s32 command, s32 unusedSecondArg);
static s32 _akropolisForkedRoadHandleRoomAction(Task* unusedTask, s32 unusedMessageId, const DirectionActionRequest* request, s32 unusedSecondArg);

/// Messages of the room task.
static TaskMessageEntry _gAkropolisForkedRoadRoomMessages[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _akropolisForkedRoadResolveRoomTransition },
    { ROOM_MESSAGE_USE_KEY_ITEM, _akropolisForkedRoadRejectKeyItemUse },
    { ROOM_MESSAGE_COMMAND, _akropolisForkedRoadHandleRoomCommand },
    { DIRECTION_MESSAGE_ROOM_ACTION, _akropolisForkedRoadHandleRoomAction },
    { TASK_MESSAGE_TABLE_END, NULL },
};

/// Set when the return ride starts; zero whenever the room is loaded.
static _ReturnRideLatch _gAkropolisForkedRoadReturnRideLatch = { 0, { 0 } };

/// The doors: the fountain's is held until MIST is contacted, then leads to its state after the red key; the
/// observatory's is held until the escalator works, then the first time starts the ride there, and after the red key
/// leads to the bat ambush until it has played.
static s32 _akropolisForkedRoadResolveRoomTransition(Task* unusedTask, s32 unusedMessageId, const RoomEventMsg* request, RoomEventMsg* reply)
{
    *reply = *request;
    if (request->areaId == AKROPOLIS_ROOM_ID_FOUNTAIN) {
        if ((gameFlagGetNibble(EVENT_FLAG_AKROPOLIS_SECURITY_ROOM_PANEL_KEYS) & AKROPOLIS_SECURITY_ROOM_PANEL_KEY_RED_INSERTED) && (request->queryOnly == ROOM_EVENT_EXECUTE)) {
            reply->roomState = AKROPOLIS_ROOM_STATE_FOUNTAIN_RED_KEY_INSERTED;
        }
        if (request->areaId == AKROPOLIS_ROOM_ID_FOUNTAIN) {
            if (gameFlagGetNibble(EVENT_FLAG_AKROPOLIS_CAFETERIA_ROUTE_PROGRESS) < AKROPOLIS_CAFETERIA_ROUTE_MIST_CONTACTED) {
                if (request->queryOnly == ROOM_EVENT_EXECUTE) {
                    gameFlagSetNibbleIfPresent(request->flagId, EVENT_FLAG_MAP_DOOR_LOCKED);
                    capStartSequenceSlot(AKROPOLIS_FORKED_ROAD_FOUNTAIN_CAP_SEQUENCE, CAP_PLAYBACK_DISPLAY_TRANSITION, 0);
                }
                return ROOM_EVENT_STAY;
            }
            if (gameFlagGetNibble(EVENT_FLAG_AKROPOLIS_CAFETERIA_ROUTE_PROGRESS) >= AKROPOLIS_CAFETERIA_ROUTE_MIST_CONTACTED) {
                if (gameFlagGetNibble(EVENT_FLAG_AKROPOLIS_FOUNTAIN_FORKED_ROAD_DOOR_OPENED) == 0) {
                    if (request->queryOnly == ROOM_EVENT_EXECUTE) {
                        capStartSequenceSlot(AKROPOLIS_FORKED_ROAD_FOUNTAIN_CAP_SEQUENCE, CAP_PLAYBACK_DISPLAY_TRANSITION, AKROPOLIS_FORKED_ROAD_FOUNTAIN_FIRST_OPEN_CAP_STEP);
                        gameFlagSetNibble(EVENT_FLAG_AKROPOLIS_FOUNTAIN_FORKED_ROAD_DOOR_OPENED, 1);
                    }
                }
                return ROOM_EVENT_DEPART;
            }
        }
    }
    if (request->areaId == AKROPOLIS_ROOM_ID_OBSERVATORY) {
        if (gameFlagGetNibble(EVENT_FLAG_AKROPOLIS_SANCTUARY_ROUTE_PROGRESS) < AKROPOLIS_SANCTUARY_ROUTE_ESCALATOR_UNLOCKED) {
            if (request->queryOnly == ROOM_EVENT_EXECUTE) {
                gameFlagSetNibbleIfPresent(request->flagId, EVENT_FLAG_MAP_DOOR_LOCKED);
                capRunCommandWithTransition(AKROPOLIS_FORKED_ROAD_OBSERVATORY_HELD_CAP_COMMAND);
            }
            return ROOM_EVENT_STAY;
        }
        if ((gDisplayState.debugMode < 0) || (gStreamInterFile.startSector == 0)) {
            if (request->queryOnly != ROOM_EVENT_EXECUTE) {
                return ROOM_EVENT_DEPART;
            }
            reply->warp = AKROPOLIS_FORKED_ROAD_OBSERVATORY_WARP;
        } else if (gameFlagGetNibble(EVENT_FLAG_AKROPOLIS_SANCTUARY_ROUTE_PROGRESS) == AKROPOLIS_SANCTUARY_ROUTE_ESCALATOR_UNLOCKED) {
            if (request->queryOnly == ROOM_EVENT_EXECUTE) {
                // Select the movie view before starting the controller that owns the departure.
                gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.view = AKROPOLIS_FORKED_ROAD_OUTBOUND_MOVIE_VIEW;
                playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
                taskSpawnFromTable(gAkropolisForkedRoadMovieTaskDescriptors, AKROPOLIS_FORKED_ROAD_TASK_OUTBOUND_MOVIE, 0, 0);
                gameFlagSetNibble(EVENT_FLAG_AKROPOLIS_SANCTUARY_ROUTE_PROGRESS, AKROPOLIS_SANCTUARY_ROUTE_OBSERVATORY_ENTERED);
            }
            return ROOM_EVENT_STAY;
        } else {
            reply->warp = AKROPOLIS_FORKED_ROAD_OBSERVATORY_WARP;
        }
        if (request->queryOnly != ROOM_EVENT_EXECUTE) {
            return ROOM_EVENT_DEPART;
        }
        if ((gameFlagGetNibble(EVENT_FLAG_AKROPOLIS_SECURITY_ROOM_PANEL_KEYS) & AKROPOLIS_SECURITY_ROOM_PANEL_KEY_RED_INSERTED) == 0) {
            return ROOM_EVENT_DEPART;
        }
        if (gameFlagGetNibble(EVENT_FLAG_AKROPOLIS_OBSERVATORY_BAT_AMBUSH_SEEN) != 0) {
            return ROOM_EVENT_DEPART;
        }
        reply->roomState = AKROPOLIS_ROOM_STATE_OBSERVATORY_BAT_AMBUSH;
        return ROOM_EVENT_DEPART;
    }
    return ROOM_EVENT_DEPART;
}

/// No key item can be used here.
static s32 _akropolisForkedRoadRejectKeyItemUse(Task* unusedTask, s32 unusedMessageId, s32 unusedItemId, s32 unusedSecondArg)
{
    return ROOM_KEY_ITEM_USE_REFUSED;
}

/// Room command 3, the dead SWAT member: CAP 3 ("He looks so young.") while his object is in state 0 or 1, else CAP 4
/// ("Something ripped through his polymer vest...").
static s32 _akropolisForkedRoadHandleRoomCommand(Task* unusedTask, s32 unusedMessageId, s32 command, s32 unusedSecondArg)
{
    enum {
        ROOM_COMMAND_DEAD_SWAT = 3,
        DEAD_SWAT_OBJECT       = 24,
        DEAD_SWAT_CAP          = 3,
        DEAD_SWAT_WOUND_CAP    = 4
    };
    s32 capCommand;

    if (command == ROOM_COMMAND_DEAD_SWAT) {
        if ((areaGetCurrentObjectState(DEAD_SWAT_OBJECT) == 0) || (areaGetCurrentObjectState(DEAD_SWAT_OBJECT) == 1)) {
            capCommand = DEAD_SWAT_CAP;
        } else {
            capCommand = DEAD_SWAT_WOUND_CAP;
        }
        capRunCommandWithTransition(capCommand);
    }
    return 0;
}

/// Room action 1 starts the ambush once, after the red key, in placement variants 4 and 8: Aya stands, the caterpillars
/// come in, and the battle starts.
static s32 _akropolisForkedRoadHandleRoomAction(Task* unusedTask, s32 unusedMessageId, const DirectionActionRequest* request, s32 unusedSecondArg)
{
    enum {
        ACTION_AMBUSH        = 1,
        AMBUSH_VARIANT       = 4,
        AMBUSH_LATER_VARIANT = 8
    };
    u8 placementVariant;

    if (request->actionId == ACTION_AMBUSH && (gameFlagGetNibble(EVENT_FLAG_AKROPOLIS_SECURITY_ROOM_PANEL_KEYS) & AKROPOLIS_SECURITY_ROOM_PANEL_KEY_RED_INSERTED)) {
        placementVariant = gGameSession->location.variant;
        if (((placementVariant == AMBUSH_VARIANT) || (placementVariant == AMBUSH_LATER_VARIANT)) && (gameFlagGetNibble(EVENT_FLAG_AKROPOLIS_FORKED_ROAD_AMBUSH_SEEN) == 0)) {
            evsStartScript(gAkropolisForkedRoadAmbushScript, EVENT_SCRIPT_HUD_KEEP);
            gameFlagSetNibble(EVENT_FLAG_AKROPOLIS_FORKED_ROAD_AMBUSH_SEEN, 1);
        }
    }
    return 1;
}

/// Installs the room's messages.
static void _akropolisForkedRoadInitializeRoomTask(Task* task)
{
    task->msgTable = _gAkropolisForkedRoadRoomMessages;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    task->state = task->state + 1;
}

/// Starts the return ride once, on arrival through its warp.
static void _akropolisForkedRoadStartReturnArrivalScene(Task* unusedTask)
{
    if ((_gAkropolisForkedRoadReturnRideLatch.spawned == 0) && (gGameSession->location.warp == AKROPOLIS_FORKED_ROAD_RETURN_ARRIVAL_WARP)) {
        _gAkropolisForkedRoadReturnRideLatch.spawned = 1;
        taskSpawnFromTable(gAkropolisForkedRoadMovieTaskDescriptors, AKROPOLIS_FORKED_ROAD_TASK_RETURN_MOVIE, 0, 0);
    }
}

void akropolisForkedRoadRoomTask(Task* task)
{
    TaskFuncTable3 stateHandlers = _gAkropolisForkedRoadRoomTaskStates;
    stateHandlers.funcs[task->state](task);
}
