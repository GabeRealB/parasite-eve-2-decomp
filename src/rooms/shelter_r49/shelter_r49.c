#include "rooms/shelter_r49.h"

#include "types.h"

#include "actors/actor_143900.h"

#include "gameplay/direction.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/message.h"

#include "main/mc.h"
#include "main/mc_types.h"
#include "main/session.h"
#include "main/task.h"
#include "main/task_types.h"

#include "mapui/map_shelter.h"

static void _shelterR49InitializeRoomTask(Task* task);
static void _shelterR49RoomIdle(Task* unusedTask);

/// Setup, idle and teardown callbacks indexed by the room task's state 0..2.
///
/// Copied by value each tick before dispatch. All three entries are callable;
/// there is no end marker or bounds check. The room must remain loaded.
static const TaskFuncTable3 _gShelterR49RoomStates = {
    { _shelterR49InitializeRoomTask, _shelterR49RoomIdle, taskKill },
};

static s32 _shelterR49RejectKeyItem(Task* unusedTask, s32 unusedMessageId, s32 itemId, s32 unusedSecondArg);
static s32 _shelterR49ResolveRoomTransition(Task* unusedTask, s32 unusedMessageId, RoomEventMsg* request, RoomEventMsg* reply);
static s32 _shelterR49IgnoreRoomCommand(Task* unusedTask, s32 unusedMessageId, s32 commandId, s32 commandMode);
static s32 _shelterR49IgnoreDirectionAction(Task* unusedTask, s32 unusedMessageId, const DirectionActionRequest* request, s32 unusedSecondArg);

/// Room-task handlers for transitions, key items, direction actions and commands.
///
/// Initialization lends the five-record table to the room task for its
/// lifetime. Transition payloads are borrowed request/reply pointers; the
/// other handlers ignore their payloads. The final marker ends unsupported lookup.
static TaskMessageEntry _gShelterR49RoomMessages[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _shelterR49ResolveRoomTransition },
    { ROOM_MESSAGE_USE_KEY_ITEM, _shelterR49RejectKeyItem },
    { DIRECTION_MESSAGE_ROOM_ACTION, _shelterR49IgnoreDirectionAction },
    { ROOM_MESSAGE_COMMAND, _shelterR49IgnoreRoomCommand },
    { TASK_MESSAGE_TABLE_END, NULL },
};

/// Refuses key-item use in Shelter R49 without changing inventory.
///
/// Ignores all arguments. The item menu supplies the collected-item ID and a
/// zero second payload; the refused reply selects its cannot-use notice.
static s32 _shelterR49RejectKeyItem(Task* unusedTask, s32 unusedMessageId, s32 itemId, s32 unusedSecondArg)
{
    return ROOM_KEY_ITEM_USE_REFUSED;
}

/// Accepts a room transition with its Mine/Shelter destination variant resolved.
///
/// Borrows complete, two-byte-aligned request and writable reply records until
/// return; they may be the same object. Copies all eight bytes, then updates the
/// destination room from game progress for an executing request. Queries retain
/// the copied destination. Requires the loaded `map_shelter` overlay and valid
/// destination selectors/progress nibbles. Task and message ID are ignored.
static s32 _shelterR49ResolveRoomTransition(Task* unusedTask, s32 unusedMessageId, RoomEventMsg* request, RoomEventMsg* reply)
{
    enum { SHELTER_R49_ROOM_TRANSITION_ACCEPTED = 1 };

    *reply = *request;
    mapShelterRoomVariantResolve(request, reply);
    return SHELTER_R49_ROOM_TRANSITION_ACCEPTED;
}

/// Ignores CAP and direction room commands, returning zero.
///
/// All arguments are ignored, including the integer command ID and mode; the
/// senders do not inspect the result.
static s32 _shelterR49IgnoreRoomCommand(Task* unusedTask, s32 unusedMessageId, s32 commandId, s32 commandMode)
{
    return 0;
}

/// Ignores action requests from direction triggers, returning zero.
///
/// The request is borrowed until return and is neither read nor retained.
/// All arguments are ignored; the sender supplies zero as the second payload
/// and does not inspect the result.
static s32 _shelterR49IgnoreDirectionAction(Task* unusedTask, s32 unusedMessageId, const DirectionActionRequest* request, s32 unusedSecondArg)
{
    return 0;
}

/// Registers the room task and starts its entry scene outside attract demo 9.
///
/// Room state 0; borrows the room message table for the task's lifetime and
/// starts the loaded entry/skip scripts with HUD restoration. Demo 9 suppresses
/// playback. Both paths advance to the idle state without allocating work.
static void _shelterR49InitializeRoomTask(Task* task)
{
    enum { SHELTER_R49_ATTRACT_DEMO_SCENE = 9 };

    task->msgTable = _gShelterR49RoomMessages;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.demoScene != SHELTER_R49_ATTRACT_DEMO_SCENE) {
        evsStartScriptWithSkip(gActor143900ObservatoryEntryScript, EVENT_SCRIPT_HUD_HIDE_RESTORE, gActor143900ObservatoryEntrySkipScript);
    }
    task->state++;
}

/// Leaves the initialized room task live until external teardown.
///
/// The unused array preserves the original idle stub's 16-byte stack frame.
static void _shelterR49RoomIdle(Task* unusedTask)
{
    char unusedStack[0x10];
}

void shelterR49RoomTask(Task* task)
{
    TaskFuncTable3 stateHandlers;

    stateHandlers = _gShelterR49RoomStates;
    stateHandlers.funcs[task->state](task);
}
