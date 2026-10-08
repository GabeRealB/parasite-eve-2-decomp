#include "rooms/shelter_b6_growth_room.h"

#include "types.h"

#include "shelter_b6_growth_room_private.h"

#include "gameplay/area_transitions.h"
#include "gameplay/captions.h"
#include "gameplay/player_state.h"
#include "gameplay/actor_presentation.h"
#include "gameplay/gameflag.h"
#include "gameplay/direction.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/message.h"

#include "main/gameflag.h"
#include "main/session.h"
#include "main/task.h"
#include "main/task_types.h"

#include "actors/actor_450900.h"

#include "mapui/map_neo_ark.h"

extern TaskDesc         D_actor_450900_80135E78[];
extern TaskMessageEntry D_shelter_b6_growth_room_8017F16C[];

extern EvsCommand D_actor_450900_80136110[];
extern EvsCommand D_actor_450900_80136308[];

static s32 _shelterB6GrowthRoomRejectKeyItemUse(Task* task, s32 messageId, s32 itemId, s32 unused);
static s32 _shelterB6GrowthRoomResolveRoomVariant(Task* task, s32 messageId, RoomEventMsg* request, RoomEventMsg* destination);
static s32 _shelterB6GrowthRoomHandleCompanionCommand(Task* unusedTask, s32 unusedMessageId, s32 command, s32 unusedSecondArg);
static s32 _shelterB6GrowthRoomHandleRoomAction(Task* unusedTask, s32 unusedMessageId, const DirectionActionRequest* request, s32 unusedSecondArg);

TaskMessageEntry D_shelter_b6_growth_room_8017F16C[5] = {
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
        SHELTER_B6_GROWTH_ROOM_COMPANION_CHOICE_TASK    = 3,
        SHELTER_B6_GROWTH_ROOM_CAP_REPEAT_CHOICE        = 1,
        SHELTER_B6_GROWTH_ROOM_CAP_BEFORE_CHOICE        = 16,
        SHELTER_B6_GROWTH_ROOM_CAP_AFTER_CHOICE         = 17
    };

    if (command == SHELTER_B6_GROWTH_ROOM_COMMAND_COMPANION_CHOICE) {
        if (gameFlagGetNibble(GAME_FLAG_0D8) == 0) {
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
            taskSpawnFromTable(D_actor_450900_80135E78, SHELTER_B6_GROWTH_ROOM_COMPANION_CHOICE_TASK, 0, 0);
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
        SHELTER_B6_GROWTH_ROOM_COMPANION_DISTRESS_TASK = 1,
        SHELTER_B6_GROWTH_ROOM_PLAYER_REACTION_TASK    = 2,
        SHELTER_B6_GROWTH_ROOM_OBJECTIVE_ON_ENTRY      = 0x33
    };

    task->msgTable = D_shelter_b6_growth_room_8017F16C;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    companionRestoreFullHp();
    areaApplySavedUpdates(D_shelter_b6_growth_room_801807C8);
    evsStartScriptWithSkip(D_actor_450900_80136110, EVENT_SCRIPT_HUD_HIDE_RESTORE, D_actor_450900_80136308);
    taskSpawnFromTable(D_actor_450900_80135E78, SHELTER_B6_GROWTH_ROOM_COMPANION_DISTRESS_TASK, 0, 0);
    taskSpawnFromTable(D_actor_450900_80135E78, SHELTER_B6_GROWTH_ROOM_PLAYER_REACTION_TASK, 0, 0);
    gameFlagSetPackedByte(GAME_FLAG_CURRENT_OBJECTIVE, SHELTER_B6_GROWTH_ROOM_OBJECTIVE_ON_ENTRY);
    task->state++;
}

/// Keeps the room task available for messages after entry setup.
static void _shelterB6GrowthRoomMessageIdle(Task* task)
{
}

/// State table of the room task: set-up, idle, kill.
static const TaskFuncTable3 D_shelter_b6_growth_room_8017D5C4 = {
    {
        _shelterB6GrowthRoomInitializeTask,
        _shelterB6GrowthRoomMessageIdle,
        taskKill,
    },
};

void shelterB6GrowthRoomTask(Task* task)
{
    TaskFuncTable3 states;

    states = D_shelter_b6_growth_room_8017D5C4;
    states.funcs[task->state](task);
}
