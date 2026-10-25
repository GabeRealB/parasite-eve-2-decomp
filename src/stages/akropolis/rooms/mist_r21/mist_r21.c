#include "stages/akropolis/rooms/mist_r21.h"

#include "../../room_ids.h"

#include "types.h"

#include "gameplay/companion_load.h"
#include "gameplay/direction.h"
#include "gameplay/message.h"

#include "main/fs.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/task.h"
#include "main/task_types.h"

#include "../../../../shared/room/room_variants.h"

static s32  _mistR21RejectKeyItemUse(Task* unusedTask, s32 unusedMessageId, s32 itemId, s32 unusedSecondArg);
static s32  _mistR21ResolveRoomTransition(Task* unusedTask, s32 unusedMessageId, const RoomEventMsg* request, RoomEventMsg* reply);
static s32  _mistR21IgnoreRoomCommand(Task* unusedTask, s32 unusedMessageId, s32 command, s32 unusedSecondArg);
static s32  _mistR21IgnoreRoomAction(Task* unusedTask, s32 unusedMessageId, const DirectionActionRequest* request, s32 unusedSecondArg);
static void _mistR21IdleAuxiliaryTask(Task* unusedTask);

/// Messages of the room task.
static TaskMessageEntry _gMistR21RoomMessages[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _mistR21ResolveRoomTransition },
    { ROOM_MESSAGE_USE_KEY_ITEM, _mistR21RejectKeyItemUse },
    { DIRECTION_MESSAGE_ROOM_ACTION, _mistR21IgnoreRoomAction },
    { ROOM_MESSAGE_COMMAND, _mistR21IgnoreRoomCommand },
    { TASK_MESSAGE_TABLE_END, NULL },
};

/// Bodyless priority-192 recipe for the persistent idle task started on room entry.
static TaskDesc _gMistR21IdleTaskDesc = {
    { { TASK_BODY_NONE, TASK_PRIORITY_CONTROL } }, _mistR21IdleAuxiliaryTask, { .value = 0 }
};

static void _mistR21InitializeRoomTask(Task* task);
static void _mistR21CheckPlazaReloadShortcut(Task* task);

/// No key item can be used here.
static s32 _mistR21RejectKeyItemUse(Task* unusedTask, s32 unusedMessageId, s32 itemId, s32 unusedSecondArg)
{
    return ROOM_KEY_ITEM_USE_REFUSED;
}

/// Every door departs as it is.
static s32 _mistR21ResolveRoomTransition(Task* unusedTask, s32 unusedMessageId, const RoomEventMsg* request, RoomEventMsg* reply)
{
    *reply = *request;
    return ROOM_EVENT_DEPART;
}

/// Room commands do nothing here.
static s32 _mistR21IgnoreRoomCommand(Task* unusedTask, s32 unusedMessageId, s32 command, s32 unusedSecondArg)
{
    return 0;
}

/// Room actions do nothing here.
static s32 _mistR21IgnoreRoomAction(Task* unusedTask, s32 unusedMessageId, const DirectionActionRequest* request, s32 unusedSecondArg)
{
    return 0;
}

/// Installs the room's messages and starts the idle auxiliary task.
static void _mistR21InitializeRoomTask(Task* task)
{
    enum { AUXILIARY_TASK_INDEX = 0 };

    task->msgTable = _gMistR21RoomMessages;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    taskSpawnFromTable(&_gMistR21IdleTaskDesc, AUXILIARY_TASK_INDEX, 0, NULL);
    task->state = task->state + 1;
}

/// A debug shortcut: holding L3 and pressing Cross reloads into the Akropolis plaza.
static void _mistR21CheckPlazaReloadShortcut(Task* task)
{
    enum { WARP = 1,
           VIEW = 2 };

    if ((padCheckButtons(0, PAD_BUTTON_QUERY_HELD_ANY, PAD_BUTTON_L3) != 0) && (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, PAD_BUTTON_CROSS) != 0)) {
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.area = AKROPOLIS_ROOM_ID_PLAZA;
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.warp = WARP;
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.view = VIEW;
        gameFlowBeginLoadScreen(&gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location, GAME_FLOW_LOAD_CAPTION_NORMAL);
        taskSpawn(GAME_FLOW_RELOAD_TASK_BANK, GAME_FLOW_RELOAD_TASK_SLOT, GAME_FLOW_RELOAD_BLANK_DISPLAY, 0);
        taskKill(task);
    }
}

/// Room task states: initialize, the plaza shortcut, release.
static const TaskFuncTable3 _gMistR21RoomTaskStates = {
    { _mistR21InitializeRoomTask, _mistR21CheckPlazaReloadShortcut, taskKill },
};

/// Retained, unused diagnostic text reporting that a target was set.
static const char _gMistR21UnusedTargetSetMessage[] = "target set\n";

void mistR21RoomTask(Task* task)
{
    TaskFuncTable3 stateHandlers;

    stateHandlers = _gMistR21RoomTaskStates;
    stateHandlers.funcs[task->state](task);
}

/// Keeps the auxiliary task spawned on room entry alive without per-frame work.
static void _mistR21IdleAuxiliaryTask(Task* unusedTask)
{
    // Unused in the binary; the original local's purpose is unproven.
    char unusedStackBytes[16];
}
