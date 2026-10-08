#include "rooms/shelter_r49.h"

#include "types.h"

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

extern TaskMessageEntry D_shelter_r49_8017D9D8[];

extern EvsCommand D_actor_143900_80133560[];
extern EvsCommand D_actor_143900_80133860[];

static void func_shelter_r49_8017D648(Task* arg0);
static void _shelterR49RoomIdle(Task* unusedTask);

/// The room task's states: set up, idle, then `taskKill`.
static const TaskFuncTable3 D_shelter_r49_8017D5C4 = {
    { func_shelter_r49_8017D648, _shelterR49RoomIdle, taskKill },
};

static s32 _shelterR49RejectKeyItem(Task* unusedTask, s32 unusedMessageId, s32 itemId, s32 unusedSecondArg);
static s32 _shelterR49ResolveRoomTransition(Task* unusedTask, s32 unusedMessageId, RoomEventMsg* request, RoomEventMsg* reply);
static s32 _shelterR49IgnoreRoomCommand(Task* unusedTask, s32 unusedMessageId, s32 commandId, s32 commandMode);
static s32 _shelterR49IgnoreDirectionAction(Task* unusedTask, s32 unusedMessageId, const DirectionActionRequest* request, s32 unusedSecondArg);

TaskMessageEntry D_shelter_r49_8017D9D8[5] = {
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

/// The room task's setup state: installs the room's message table, stores the
/// task in pointer slot 7 and, unless `gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.demoScene` is 9, calls
/// `evsStartScriptWithSkip`.
static void func_shelter_r49_8017D648(Task* arg0)
{
    arg0->msgTable = D_shelter_r49_8017D9D8;
    gameSetTaskSlot(arg0, GAME_TASK_SLOT_ROOM);
    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.demoScene != 9) {
        evsStartScriptWithSkip(D_actor_143900_80133560, EVENT_SCRIPT_HUD_HIDE_RESTORE, D_actor_143900_80133860);
    }
    arg0->state = (s32)(arg0->state + 1);
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

    stateHandlers = D_shelter_r49_8017D5C4;
    stateHandlers.funcs[task->state](task);
}
