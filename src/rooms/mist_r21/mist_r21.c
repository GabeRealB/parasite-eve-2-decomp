#include "rooms/mist_r21.h"

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

#include "../../shared/room_variants.h"

static s32  _mistR21RejectKeyItemUse(Task* unusedTask, s32 unusedMessageId, s32 itemId, s32 unusedSecondArg);
static s32  _mistR21ResolveRoomTransition(Task* unusedTask, s32 unusedMessageId, const RoomEventMsg* request, RoomEventMsg* reply);
static s32  _mistR21IgnoreRoomCommand(Task* unusedTask, s32 unusedMessageId, s32 command, s32 unusedSecondArg);
static s32  _mistR21IgnoreRoomAction(Task* unusedTask, s32 unusedMessageId, const DirectionActionRequest* request, s32 unusedSecondArg);
static void _mistR21IdleAuxiliaryTask(Task* unusedTask);

/// The room's message table, published at `Task::msgTable` by the room task.
TaskMessageEntry D_mist_r21_8017D770[] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _mistR21ResolveRoomTransition },
    { ROOM_MESSAGE_USE_KEY_ITEM, _mistR21RejectKeyItemUse },
    { DIRECTION_MESSAGE_ROOM_ACTION, _mistR21IgnoreRoomAction },
    { ROOM_MESSAGE_COMMAND, _mistR21IgnoreRoomCommand },
    { TASK_MESSAGE_TABLE_END, NULL },
};

/// The one task the room task spawns on entry; its callback is the empty
/// `_mistR21IdleAuxiliaryTask`.
TaskDesc D_mist_r21_8017D798[] = {
    { { { TASK_BODY_NONE, 0xC0 } }, _mistR21IdleAuxiliaryTask, { .value = 0 } },
};

static void func_mist_r21_8017D61C(Task* task);
static void func_mist_r21_8017D678(Task* task);

/// Refuses key-item use in this M.I.S.T. room without consuming the item.
///
/// `ROOM_MESSAGE_USE_KEY_ITEM` supplies a collected item ID and a zero second
/// word. All arguments are unused; the reply makes the item menu show refusal.
static s32 _mistR21RejectKeyItemUse(Task* unusedTask, s32 unusedMessageId, s32 itemId, s32 unusedSecondArg)
{
    return ROOM_KEY_ITEM_USE_REFUSED;
}

/// Permits departure from this M.I.S.T. room without changing its destination.
///
/// `ROOM_EVENT_MESSAGE_RESOLVE` borrows a readable eight-byte request and a
/// writable reply, which may alias. Copies the complete record in query and
/// execution modes, retains no pointers and always permits ordinary departure.
static s32 _mistR21ResolveRoomTransition(Task* unusedTask, s32 unusedMessageId, const RoomEventMsg* request, RoomEventMsg* reply)
{
    *reply = *request;
    return ROOM_VARIANT_TRANSITION_DIRECT;
}

/// Ignores CAP room commands in this M.I.S.T. room.
///
/// `ROOM_MESSAGE_COMMAND` supplies an integer command and a second payload word;
/// all arguments are unused. Returns 0 without changing room or task state.
static s32 _mistR21IgnoreRoomCommand(Task* unusedTask, s32 unusedMessageId, s32 command, s32 unusedSecondArg)
{
    return 0;
}

/// Ignores trigger requests for a room-specific action in this M.I.S.T. room.
///
/// `DIRECTION_MESSAGE_ROOM_ACTION` supplies a borrowed four-byte request and a
/// zero second word. All arguments are unused; returns 0, ignored by the sender.
static s32 _mistR21IgnoreRoomAction(Task* unusedTask, s32 unusedMessageId, const DirectionActionRequest* request, s32 unusedSecondArg)
{
    return 0;
}

/// First state of the room task: publishes the room's message table, claims
/// pointer slot 7, spawns the task `D_mist_r21_8017D798` describes and
/// advances to the next state.
static void func_mist_r21_8017D61C(Task* task)
{
    task->msgTable = D_mist_r21_8017D770;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    taskSpawnFromTable(D_mist_r21_8017D798, 0, 0, 0);
    task->state = (s32)(task->state + 1);
}

/// Second state of the room task: waits for pad 0 to report button 0x200 in
/// mode 0 and button 0x40 in mode 1, then sets the saved location to area 5,
/// warp 1, view 2, starts loading from it, spawns task 0x11 and ends itself.
static void func_mist_r21_8017D678(Task* task)
{
    if ((padCheckButtons(0, PAD_BUTTON_QUERY_HELD_ANY, PAD_BUTTON_L3) != 0) && (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, PAD_BUTTON_CROSS) != 0)) {
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.area = GAME_AREA_ACROPOLIS_PLAZA;
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.warp = 1;
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = 2;
        gameFlowBeginLoadScreen(&gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc, GAME_FLOW_LOAD_CAPTION_NORMAL);
        taskSpawn(GAME_FLOW_RELOAD_TASK_BANK, GAME_FLOW_RELOAD_TASK_SLOT, GAME_FLOW_RELOAD_BLANK_DISPLAY, 0);
        taskKill(task);
    }
}

/// The room task's three states.
static const TaskFuncTable3 D_mist_r21_8017D5C4 = {
    { func_mist_r21_8017D61C, func_mist_r21_8017D678, taskKill },
};

/// `"target set\n"`: no code in the room reads it.
static const char D_mist_r21_8017D5D0[] = "target set\n";

/// The room task's callback: runs the state `Task::state` selects from a
/// stack copy of `D_mist_r21_8017D5C4`.
void func_mist_r21_8017D708(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_mist_r21_8017D5C4;
    sp.funcs[task->state](task);
}

/// Keeps the auxiliary task spawned on room entry alive without per-frame work.
///
/// The task, its work and spawn arguments are unused; this callback does not
/// advance its state or end it.
static void _mistR21IdleAuxiliaryTask(Task* unusedTask)
{
    // Unused in the binary; the original local's purpose is unproven.
    char unusedStackBytes[16];
}
