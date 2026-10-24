#include "rooms/shelter_b6_growth_room.h"

#include "types.h"

#include "shelter_b6_growth_room_private.h"

#include "gameplay/area_transitions.h"
#include "gameplay/captions.h"
#include "gameplay/player_state.h"
#include "gameplay/actor_presentation.h"
#include "gameplay/gameflag.h"
#include "gameplay/direction.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/message.h"

#include "main/gameflag.h"
#include "main/session.h"
#include "main/task.h"
#include "main/task_types.h"

#include "actors/actor_450900.h"

#include "mapui/map_neo_ark.h"

static s32 _shelterB6GrowthRoomRejectKeyItemUse(Task* task, s32 messageId, s32 itemId, s32 unused);
static s32 _shelterB6GrowthRoomResolveRoomVariant(Task* task, s32 messageId, RoomEventMsg* request, RoomEventMsg* destination);
static s32 _shelterB6GrowthRoomHandleCompanionCommand(Task* unusedTask, s32 unusedMessageId, s32 command, s32 unusedSecondArg);
static s32 _shelterB6GrowthRoomHandleRoomAction(Task* unusedTask, s32 unusedMessageId, const DirectionActionRequest* request, s32 unusedSecondArg);

/// Room-message dispatch for Neo Ark transitions, key-item refusals and companion interaction/departure.
///
/// Four id-selected handlers followed by TASK_MESSAGE_TABLE_END. The room
/// controller borrows the table for synchronous dispatch; keep the overlay
/// loaded while that controller can receive messages.
static TaskMessageEntry _gShelterB6GrowthRoomMessages[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _shelterB6GrowthRoomResolveRoomVariant },
    { ROOM_MESSAGE_USE_KEY_ITEM, _shelterB6GrowthRoomRejectKeyItemUse },
    { DIRECTION_MESSAGE_ROOM_ACTION, _shelterB6GrowthRoomHandleRoomAction },
    { ROOM_MESSAGE_COMMAND, _shelterB6GrowthRoomHandleCompanionCommand },
    { TASK_MESSAGE_TABLE_END, NULL },
};

static void _shelterB6GrowthRoomInitializeTask(Task* task);
static void _shelterB6GrowthRoomMessageIdle(Task* task);

/// Refuses key-item use in the growth room without consuming the item.
static s32 _shelterB6GrowthRoomRejectKeyItemUse(Task* task, s32 messageId, s32 itemId, s32 unused)
{
    return ROOM_KEY_ITEM_USE_REFUSED;
}

/// Resolves a room-transition destination from Neo Ark's saved progress.
///
/// Borrows an eight-byte request and writable destination for this dispatch;
/// they may alias. Copies the complete request, resolves its room variant and
/// returns 1. The records are not retained.
static s32 _shelterB6GrowthRoomResolveRoomVariant(Task* task, s32 messageId, RoomEventMsg* request, RoomEventMsg* destination)
{
    *destination = *request;
    mapNeoArkResolveRoomVariant(request, destination);
    return 1;
}

/// Selects the companion interaction or its progress-dependent idle CAP event.
///
/// Handles `ROOM_MESSAGE_COMMAND`: command 1 holds player control and starts
/// the choice task until its confirming choice has been recorded, then uses
/// the repeat command. Command 16 requests CAP event 16 or 17 only when idle.
/// Other commands have no effect. Returns zero and ignores the second word.
/// The actor overlay and current companion/player must remain live for the scene.
static s32 _shelterB6GrowthRoomHandleCompanionCommand(Task* unusedTask, s32 unusedMessageId, s32 command, s32 unusedSecondArg)
{
    enum {
        SHELTER_B6_GROWTH_ROOM_COMMAND_COMPANION_CHOICE = 1,
        SHELTER_B6_GROWTH_ROOM_COMMAND_IDLE_EVENT       = 16,
        SHELTER_B6_GROWTH_ROOM_CAP_REPEAT_CHOICE        = 1,
        SHELTER_B6_GROWTH_ROOM_CAP_BEFORE_CHOICE        = 16,
        SHELTER_B6_GROWTH_ROOM_CAP_AFTER_CHOICE         = 17
    };

    if (command == SHELTER_B6_GROWTH_ROOM_COMMAND_COMPANION_CHOICE) {
        if (gameFlagGetNibble(GAME_FLAG_0D8) == 0) {
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
            taskSpawnFromTable(gActor450900Tasks, ACTOR_450900_TASK_CHOICE_ACTION, 0, 0);
        } else {
            capRunCommandWithTransition(SHELTER_B6_GROWTH_ROOM_CAP_REPEAT_CHOICE);
        }
    }
    if (command == SHELTER_B6_GROWTH_ROOM_COMMAND_IDLE_EVENT) {
        capSpawnEventIfIdle(gameFlagGetNibble(GAME_FLAG_0D8) == 0 ? SHELTER_B6_GROWTH_ROOM_CAP_BEFORE_CHOICE : SHELTER_B6_GROWTH_ROOM_CAP_AFTER_CHOICE, CAP_EVENT_NO_FLAGS);
    }
    return 0;
}

/// Routes growth-room triggers to companion conversation or departure.
///
/// `DIRECTION_MESSAGE_ROOM_ACTION` borrows a four-byte, two-byte-aligned
/// request until dispatch returns. Action 1 starts conversation; action 2
/// checks departure. Other IDs have no effect. Ignores control, argument and
/// the zero second word, retains no request pointer and returns zero.
static s32 _shelterB6GrowthRoomHandleRoomAction(Task* unusedTask, s32 unusedMessageId, const DirectionActionRequest* request, s32 unusedSecondArg)
{
    enum {
        SHELTER_B6_GROWTH_ROOM_ACTION_CONVERSATION = 1,
        SHELTER_B6_GROWTH_ROOM_ACTION_DEPARTURE    = 2
    };

    if (request->actionId == SHELTER_B6_GROWTH_ROOM_ACTION_CONVERSATION) {
        actor450900StartCompanionConversation();
    }
    if (request->actionId == SHELTER_B6_GROWTH_ROOM_ACTION_DEPARTURE) {
        actor450900HandleDepartureTrigger();
    }
    return 0;
}

/// Initializes the growth-room controller and starts the companion entry scene.
///
/// State 0 registers room messages, restores companion HP and saved area
/// changes, then starts the skippable scene and its two reaction tasks.
/// Requires the actor overlay, companion and player to be loaded. Sets the
/// entry objective and advances to state 1, which remains available for messages.
static void _shelterB6GrowthRoomInitializeTask(Task* task)
{
    enum {
        SHELTER_B6_GROWTH_ROOM_OBJECTIVE_ON_ENTRY = 0x33
    };

    task->msgTable = _gShelterB6GrowthRoomMessages;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    companionRestoreFullHp();
    areaApplySavedUpdates(gShelterB6GrowthRoomEntryAreaUpdates);
    evsStartScriptWithSkip(gActor450900EntryScript, EVENT_SCRIPT_HUD_HIDE_RESTORE, gActor450900EntrySkipScript);
    taskSpawnFromTable(gActor450900Tasks, ACTOR_450900_TASK_COMPANION_DISTRESS, 0, 0);
    taskSpawnFromTable(gActor450900Tasks, ACTOR_450900_TASK_PLAYER_REACTION, 0, 0);
    gameFlagSetPackedByte(EVENT_FLAG_CURRENT_OBJECTIVE, SHELTER_B6_GROWTH_ROOM_OBJECTIVE_ON_ENTRY);
    task->state++;
}

/// Keeps the room task available for messages after entry setup.
static void _shelterB6GrowthRoomMessageIdle(Task* task)
{
}

/// Room-task states 0 initialize, 1 remain available for messages, and 2 release the task.
///
/// Copied whole before unchecked state dispatch in 0..2; all three callback
/// addresses are borrowed from loaded code. There is no terminator.
static const TaskFuncTable3 _gShelterB6GrowthRoomTaskStates = {
    {
        _shelterB6GrowthRoomInitializeTask,
        _shelterB6GrowthRoomMessageIdle,
        taskKill,
    },
};

void shelterB6GrowthRoomTask(Task* task)
{
    TaskFuncTable3 states;

    states = _gShelterB6GrowthRoomTaskStates;
    states.funcs[task->state](task);
}
