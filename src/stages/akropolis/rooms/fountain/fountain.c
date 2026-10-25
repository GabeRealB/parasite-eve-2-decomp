#include "stages/akropolis/rooms/fountain.h"

#include "config.h"
#include "../../event_flags.h"
#include "../../room_states.h"
#include "fountain_private.h"

#include "types.h"

#include "gameplay/captions.h"
#include "gameplay/companion_load.h"
#include "gameplay/message.h"

#include "main/display.h"
#include "main/display_types.h"
#include "main/event_flags.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"

static void _akropolisFountainInitializeRoomTask(Task* task);
static void _akropolisFountainIdleRoomTask(Task* unusedTask);

/// Room task states: initialize, idle, release.
static const TaskFuncTable3 _gAkropolisFountainRoomTaskStates = {
    { _akropolisFountainInitializeRoomTask, _akropolisFountainIdleRoomTask, taskKill },
};

static s32  _akropolisFountainResolveRoomTransition(Task* unusedTask, s32 unusedMessageId, const RoomEventMsg* request, RoomEventMsg* reply);
static s32  _akropolisFountainRejectKeyItemUse(Task* unusedTask, s32 unusedMessageId, s32 unusedItemId, s32 unusedSecondArg);
static s32  _akropolisFountainHandleRoomCommand(Task* unusedTask, s32 unusedMessageId, s32 command, s32 unusedSecondArg);
static s32  _akropolisFountainHandleSoundCue(Task* unusedTask, s32 unusedMessageId, s32 cueIndex, s32 unusedSecondArg);
static void _akropolisFountainPatioDepartureTask(Task* task);

/// Messages of the room task.
static TaskMessageEntry _gAkropolisFountainRoomMessages[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _akropolisFountainResolveRoomTransition },
    { ROOM_MESSAGE_COMMAND, _akropolisFountainHandleRoomCommand },
    { ROOM_MESSAGE_USE_KEY_ITEM, _akropolisFountainRejectKeyItemUse },
    { ROOM_MESSAGE_SOUND, _akropolisFountainHandleSoundCue },
    { TASK_MESSAGE_TABLE_END, NULL },
};

/// The patio departure's task.
static TaskDesc _gAkropolisFountainPatioDepartureTaskDescriptors[2] = {
    { { { TASK_BODY_NONE, TASK_PRIORITY_SCENE } }, _akropolisFountainPatioDepartureTask, { .value = 0 } },
    { { { TASK_DESC_END, 0 } }, NULL, { .model = NULL } },
};

/// The doors: the patio's is barred from this side, and the first departure unbars it (CAP 1) and reloads the patio;
/// after that it leads to the patio's state for the story so far; the forked road's leads to its state after the blue key.
static s32 _akropolisFountainResolveRoomTransition(Task* unusedTask, s32 unusedMessageId, const RoomEventMsg* request, RoomEventMsg* reply)
{
    enum { PATIO_DEPARTURE_TASK_ENTRY = 0 };
    s32 destinationAreaId;

    *reply            = *request;
    destinationAreaId = request->areaId;
    if (destinationAreaId == AKROPOLIS_ROOM_ID_PATIO) {
        if (gameFlagGetNibble(EVENT_FLAG_AKROPOLIS_CAFETERIA_ROUTE_PROGRESS) < AKROPOLIS_CAFETERIA_ROUTE_FOUNTAIN_UNLOCKED) {
            if (request->queryOnly == ROOM_EVENT_EXECUTE) {
                gAkropolisFountainPatioArrivalWarp   = request->warp;
                gAkropolisFountainPatioRequestedRoom = request->roomState;
                taskSpawnFromTable(_gAkropolisFountainPatioDepartureTaskDescriptors, PATIO_DEPARTURE_TASK_ENTRY, 0, 0);
            }
            return ROOM_EVENT_STAY;
        }
        if (request->areaId == destinationAreaId && request->queryOnly == ROOM_EVENT_EXECUTE) {
            if (gameFlagGetNibble(EVENT_FLAG_AKROPOLIS_CAFETERIA_ROUTE_PROGRESS) < AKROPOLIS_CAFETERIA_ROUTE_BOSS_DEFEATED) {
                if (gameFlagGetNibble(EVENT_FLAG_AKROPOLIS_PATIO_CUTSCENE_PROGRESS) <= AKROPOLIS_PATIO_CUTSCENE_START) {
                    reply->roomState = AKROPOLIS_ROOM_STATE_PATIO_PRE_SWAT_CUTSCENE;
                } else {
                    reply->roomState = AKROPOLIS_ROOM_STATE_PATIO_POST_SWAT_CUTSCENE;
                }
            } else {
                // The patio's room id, 3, is also its state after the boss.
                reply->roomState = destinationAreaId;
            }
        }
    } else if (destinationAreaId == AKROPOLIS_ROOM_ID_FORKED_ROAD) {
        if (gameFlagGetNibble(EVENT_FLAG_AKROPOLIS_SECURITY_ROOM_PANEL_KEYS) & AKROPOLIS_SECURITY_ROOM_PANEL_KEY_BLUE_INSERTED) {
            reply->roomState = AKROPOLIS_ROOM_STATE_FORKED_ROAD_ESCALATOR_OPERATIONAL;
        }
        if (request->queryOnly == ROOM_EVENT_EXECUTE && gameFlagGetNibble(EVENT_FLAG_AKROPOLIS_FOUNTAIN_FORKED_ROAD_DOOR_OPENED) == 0) {
            gameFlagSetNibble(EVENT_FLAG_AKROPOLIS_FOUNTAIN_FORKED_ROAD_DOOR_OPENED, 1);
        }
    }
    return ROOM_EVENT_DEPART;
}

/// No key item can be used here.
static s32 _akropolisFountainRejectKeyItemUse(Task* unusedTask, s32 unusedMessageId, s32 unusedItemId, s32 unusedSecondArg)
{
    return ROOM_KEY_ITEM_USE_REFUSED;
}

/// Room command 3 examines the fountain, by whether the red key is in; 4 finds the hidden path.
static s32 _akropolisFountainHandleRoomCommand(Task* unusedTask, s32 unusedMessageId, s32 command, s32 unusedSecondArg)
{
    // Removing this unused reservation changes the original call frame.
    s32 retainedStackSpace[2];

    if (command == AKROPOLIS_FOUNTAIN_ROOM_COMMAND_FOUNTAIN) {
        capRunCommandWithTransition(((gameFlagGetNibble(EVENT_FLAG_AKROPOLIS_SECURITY_ROOM_PANEL_KEYS) & AKROPOLIS_SECURITY_ROOM_PANEL_KEY_RED_INSERTED) == 0) ? AKROPOLIS_FOUNTAIN_CAP_FOUNTAIN : AKROPOLIS_FOUNTAIN_CAP_FOUNTAIN_SWAT);
    }
    if (command == AKROPOLIS_FOUNTAIN_ROOM_COMMAND_HIDDEN_PATH) {
        capStartSequenceSlot(AKROPOLIS_FOUNTAIN_CAP_HIDDEN_PATH, CAP_PLAYBACK_DISPLAY_TRANSITION, 0);
        akropolisFountainEnableClimbTrigger();
        gameFlagSetNibble(EVENT_FLAG_AKROPOLIS_FOUNTAIN_HIDDEN_PATH_FOUND, 1);
    }
    return 0;
}

/// Plays room sound cues 3, 4 and 9: the ammo case opening, the door unbarred, the case closing.
static s32 _akropolisFountainHandleSoundCue(Task* unusedTask, s32 unusedMessageId, s32 cueIndex, s32 unusedSecondArg)
{
    switch (cueIndex) {
        case AKROPOLIS_FOUNTAIN_SOUND_AMMO_CASE_OPEN:
            sndEvtRequestScriptStart(AKROPOLIS_FOUNTAIN_SOUND(AKROPOLIS_FOUNTAIN_SOUND_AMMO_CASE_OPEN), 0, 0);
            break;
        case AKROPOLIS_FOUNTAIN_SOUND_DOOR_UNBAR:
            sndEvtRequestScriptStart(AKROPOLIS_FOUNTAIN_SOUND(AKROPOLIS_FOUNTAIN_SOUND_DOOR_UNBAR), 0, 0);
            break;
        case AKROPOLIS_FOUNTAIN_SOUND_AMMO_CASE_CLOSE:
            sndEvtRequestScriptStart(AKROPOLIS_FOUNTAIN_SOUND(AKROPOLIS_FOUNTAIN_SOUND_AMMO_CASE_CLOSE), 0, 0);
            break;
    }
    return 0;
}

/// Leaves for the patio, in its state after the boss, and records the door unbarred.
static inline void _akropolisFountainReloadPatio(Task* task)
{
    enum { PATIO_SPRITE_VARIANT = 1 };

    sndEvtRequestScriptStop(SOUND_BANK_TYPE_ALL_NON_AMBIENT, SOUND_SCRIPT_STOP_NO_FADE);
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.area      = AKROPOLIS_ROOM_ID_PATIO;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.roomState = AKROPOLIS_ROOM_STATE_PATIO_POST_CAFETERIA_BOSS;
    gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.warp      = gAkropolisFountainPatioArrivalWarp;
    gDisplayState.spriteVariant                                 = PATIO_SPRITE_VARIANT;
    taskSpawn(GAME_FLOW_RELOAD_TASK_BANK, GAME_FLOW_RELOAD_TASK_SLOT, GAME_FLOW_RELOAD_CAPTURE_FRAME, 0);
    gameFlagSetNibble(EVENT_FLAG_AKROPOLIS_CAFETERIA_ROUTE_PROGRESS, AKROPOLIS_CAFETERIA_ROUTE_FOUNTAIN_UNLOCKED);
    taskKill(task);
}

/// The first patio departure: unbarring the door (CAP 1), then the reload, on the next update even if the caption is
/// still playing.
static void _akropolisFountainPatioDepartureTask(Task* task)
{
    enum {
        START_CAP = 0,
        CHECK_CAP = 1,
        RELOAD    = 2,
    };

    switch (task->state) {
        case START_CAP:
            capRunCommandWithTransition(AKROPOLIS_FOUNTAIN_CAP_UNBAR_PATIO_DOOR);
            task->state = task->state + 1;
            break;

        case CHECK_CAP:
            if (capIsBusy() == 0) {
                task->state = task->state + 1;
            }
            // Departure proceeds on this update regardless of CAP's busy result.
            /* fallthrough */

        case RELOAD:
            _akropolisFountainReloadPatio(task);
            break;
    }
}

/// Installs the room's messages, and the climb trigger once the hidden path is found.
static void _akropolisFountainInitializeRoomTask(Task* task)
{
    task->msgTable = _gAkropolisFountainRoomMessages;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    if (gameFlagGetNibble(EVENT_FLAG_AKROPOLIS_FOUNTAIN_HIDDEN_PATH_FOUND) != 0) {
        akropolisFountainEnableClimbTrigger();
    }
    task->state = task->state + 1;
}

/// The room task's idle state.
static void _akropolisFountainIdleRoomTask(Task* unusedTask)
{
}

void akropolisFountainRoomTask(Task* task)
{
    TaskFuncTable3 stateHandlers = _gAkropolisFountainRoomTaskStates;
    stateHandlers.funcs[task->state](task);
}
