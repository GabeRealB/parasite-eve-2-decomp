#include "rooms/dryfield_night_r08.h"

#include "types.h"

#include "gameplay/direction.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/message.h"
#include "gameplay/scene_runtime.h"

#include "main/fs.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/session.h"
#include "main/task.h"
#include "main/task_types.h"

extern EvsCommand D_actor_535700_80133898[];
extern EvsCommand D_actor_535700_801341E0[];
/// The room's message table, published in `Task::msgTable` for
/// `taskMessageDispatch` to walk: 0x13EE, 0x13F1, 0x13EF and 0x13F0.
extern TaskMessageEntry D_dryfield_night_r08_80180544[];

static s32 _dryfieldNightR08RejectKeyItemUse(Task* unusedTask, s32 unusedMessageId, s32 itemId, s32 unusedSecondArg);
static s32 _dryfieldNightR08ResolveRoomEvent(Task* unusedTask, s32 unusedMessageId, const RoomEventMsg* request, RoomEventMsg* reply);
static s32 _dryfieldNightR08IgnoreCommandMessage(Task* unusedTask, s32 unusedMessageId, s32 commandId, s32 unusedSecondArg);
static s32 _dryfieldNightR08IgnoreActionMessage(Task* unusedTask, s32 unusedMessageId, const DirectionActionRequest* request, s32 unusedSecondArg);

TaskMessageEntry D_dryfield_night_r08_80180544[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, _dryfieldNightR08ResolveRoomEvent },
    { ROOM_MESSAGE_USE_KEY_ITEM, _dryfieldNightR08RejectKeyItemUse },
    { DIRECTION_MESSAGE_ROOM_ACTION, _dryfieldNightR08IgnoreActionMessage },
    { ROOM_MESSAGE_COMMAND, _dryfieldNightR08IgnoreCommandMessage },
    { TASK_MESSAGE_TABLE_END, NULL },
};

static void _dryfieldNightR08InitializeRoom(Task* task);
static void _dryfieldNightR08IdleState(Task* unusedTask);

/// Refuses every key-item use without consuming the selected inventory item.
///
/// Handles `ROOM_MESSAGE_USE_KEY_ITEM`; all arguments are ignored. The refused
/// reply makes the inventory menu display its cannot-use notice.
static s32 _dryfieldNightR08RejectKeyItemUse(Task* unusedTask, s32 unusedMessageId, s32 itemId, s32 unusedSecondArg)
{
    return ROOM_KEY_ITEM_USE_REFUSED;
}

/// Allows a room transition with the requested destination unchanged.
///
/// Copies the complete eight-byte request and returns 1 for both query and
/// execution. Both records must remain live through synchronous dispatch;
/// `reply` must be writable and may be `request`. No pointer is retained and
/// no transition effects are performed. The task and message ID are ignored.
static s32 _dryfieldNightR08ResolveRoomEvent(Task* unusedTask, s32 unusedMessageId, const RoomEventMsg* request, RoomEventMsg* reply)
{
    enum { DRYFIELD_NIGHT_R08_TRANSITION_ALLOWED = 1 };

    *reply = *request;
    return DRYFIELD_NIGHT_R08_TRANSITION_ALLOWED;
}

/// Ignores every room command and returns zero without changing room state.
///
/// `ROOM_MESSAGE_COMMAND` supplies an integer command ID; no argument is read.
static s32 _dryfieldNightR08IgnoreCommandMessage(Task* unusedTask, s32 unusedMessageId, s32 commandId, s32 unusedSecondArg)
{
    return 0;
}

/// Ignores every trigger request for a room action and returns zero.
///
/// `DIRECTION_MESSAGE_ROOM_ACTION` borrows `request` through synchronous
/// dispatch. No argument is read or retained and no action is started.
static s32 _dryfieldNightR08IgnoreActionMessage(Task* unusedTask, s32 unusedMessageId, const DirectionActionRequest* request, s32 unusedSecondArg)
{
    return 0;
}

/// Registers the room receiver and starts its skippable entry scene.
///
/// State zero publishes borrowed scene-payload storage 131072 bytes after the
/// second actor-load base and starts the entry script unless demo scene 9 is
/// selected, then advances to idle state 1. The caller must keep that arena
/// writable and free of overlapping occupants through scene loading/playback;
/// its available capacity is not established here.
static void _dryfieldNightR08InitializeRoom(Task* task)
{
    enum {
        DRYFIELD_NIGHT_R08_SCENE_PAYLOAD_BYTE_OFFSET = 0x20000,
        DRYFIELD_NIGHT_R08_DEMO_SCENE                = 9,
    };

    task->msgTable = D_dryfield_night_r08_80180544;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    streamSetExternalScenePayloadBuffer((u8*)Fs_ActorLoadBase1 + DRYFIELD_NIGHT_R08_SCENE_PAYLOAD_BYTE_OFFSET);
    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.demoScene != DRYFIELD_NIGHT_R08_DEMO_SCENE) {
        evsStartScriptWithSkip(D_actor_535700_80133898, EVENT_SCRIPT_HUD_HIDE_RESTORE, D_actor_535700_801341E0);
    }
    task->state = task->state + 1;
}

/// Keeps the initialized room task alive to receive messages in state 1.
static void _dryfieldNightR08IdleState(Task* unusedTask)
{
    char unusedFrame[0x10]; // Retains the original stack reservation; no bytes are accessed.
}

/// The room task's three states: set up, idle, then `taskKill`.
static const TaskFuncTable3 D_dryfield_night_r08_8017D5C4 = {
    { _dryfieldNightR08InitializeRoom, _dryfieldNightR08IdleState, taskKill },
};

void dryfieldNightR08RoomTask(Task* task)
{
    TaskFuncTable3 stateHandlers;

    stateHandlers = D_dryfield_night_r08_8017D5C4;
    stateHandlers.funcs[task->state](task);
}
