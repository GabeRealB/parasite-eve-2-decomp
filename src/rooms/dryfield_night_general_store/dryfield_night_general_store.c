#include "rooms/dryfield_night_general_store.h"

#include "types.h"

#include "dryfield_night_general_store_private.h"

#include "gameplay/captions.h"
#include "gameplay/actor_presentation.h"
#include "gameplay/gameflag.h"
#include "gameplay/player_actor.h"
#include "gameplay/sound.h"
#include "gameplay/direction.h"
#include "gameplay/display.h"
#include "gameplay/items.h"
#include "gameplay/message.h"
#include "gameplay/object_task.h"
#include "gameplay/scene_combat.h"

#include "main/display.h"
#include "main/display_types.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"

#include "rooms/room_common.h"
#include "../../shared/room_events.h"
#include "../../shared/general_store.h"

static s32 _generalStoreSoundMsg(Task* unusedTask, s32 unusedMessageId, s32 cueKey, s32 unusedSecondArg);

/// Descriptor of the event task `roomEventTask`.
extern TaskDesc gRoomEventTaskDesc;

/// The room's two spawnable tasks: entry 0 the CAP-command task
/// `storeToggleTask`, entry 1 the cutscene task
/// `storeCutsceneTask`.
extern TaskDesc gStoreTaskDescs[];

/// The room's message table, installed by the room task's entry state.
extern TaskMessageEntry D_dryfield_night_general_store_8017E7BC[];

static void func_dryfield_night_general_store_8017DE34(Task* arg0);
static void func_dryfield_night_general_store_8017DE80(Task* task);

static s32 _dryfieldNightGeneralStoreRejectKeyItem(Task* unusedTask, s32 unusedMessageId, s32 itemId, s32 unusedSecondArg);
s32        func_dryfield_night_general_store_8017DE2C(Task*, s32, s32, s32);

TaskDesc gRoomEventTaskDesc = { { { TASK_BODY_NONE, 32 } }, roomEventTask, { .value = 0 } };

TaskDesc gStoreTaskDescs[3] = {
    { { { TASK_BODY_NONE, 32 } }, storeToggleTask, { .value = 0 } },
    { { { TASK_BODY_NONE, 32 } }, storeCutsceneTask, { .value = 0 } },
    { { { TASK_DESC_END, 0 } }, NULL, { .model = NULL } },
};

TaskMessageEntry D_dryfield_night_general_store_8017E7BC[6] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, storeDoorMsg },
    { ROOM_MESSAGE_USE_KEY_ITEM, _dryfieldNightGeneralStoreRejectKeyItem },
    { DIRECTION_MESSAGE_ROOM_ACTION, func_dryfield_night_general_store_8017DE2C },
    { ROOM_MESSAGE_COMMAND, storeActionMsg },
    { ROOM_MESSAGE_SOUND, _generalStoreSoundMsg },
    { TASK_MESSAGE_TABLE_END, NULL },
};

#include "../../shared/room_event_gate.inc.c"

#include "../../shared/room_event_task.inc.c"

#include "../../shared/general_store_door_msg.inc.c"

#include "../../shared/general_store_cutscene_task.inc.c"

/// The room task's three-state table, run from a stack copy by
/// `func_dryfield_night_general_store_8017DE88`: the entry state
/// `func_dryfield_night_general_store_8017DE34`, the idle state
/// `func_dryfield_night_general_store_8017DE80`, then `taskKill`.
static const TaskFuncTable3 D_dryfield_night_general_store_8017D5F4 = {
    { func_dryfield_night_general_store_8017DE34, func_dryfield_night_general_store_8017DE80, taskKill },
};

#include "../../shared/general_store_toggle_task.inc.c"

#include "../../shared/general_store_action_msg.inc.c"

#include "../../shared/general_store_sound_msg.inc.c"

/// Refuses key-item use in the night General Store without changing inventory.
///
/// All arguments are ignored. The item menu supplies the selected collected-item
/// ID as `itemId` and zero as `unusedSecondArg`; the refused reply shows its
/// cannot-use notice.
static s32 _dryfieldNightGeneralStoreRejectKeyItem(Task* unusedTask, s32 unusedMessageId, s32 itemId, s32 unusedSecondArg)
{
    return ROOM_KEY_ITEM_USE_REFUSED;
}

/// Message handler that takes no action and reports the message as not
/// handled.
s32 func_dryfield_night_general_store_8017DE2C(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}

/// Entry state of the room task: installs the room's message table, publishes
/// the task in pointer slot 7, advances to the idle state and raises
/// `D_80115598`.
static void func_dryfield_night_general_store_8017DE34(Task* arg0)
{
    arg0->msgTable = D_dryfield_night_general_store_8017E7BC;
    gameSetTaskSlot(arg0, GAME_TASK_SLOT_ROOM);
    arg0->state = (s32)(arg0->state + 1);
    D_80115598  = 1;
}

/// Idle state of the room task: does nothing.
static void func_dryfield_night_general_store_8017DE80(Task* task)
{
}

/// The room task: runs the state `D_dryfield_night_general_store_8017D5F4`
/// names for `task->state`, through a stack copy of the table.
void func_dryfield_night_general_store_8017DE88(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_night_general_store_8017D5F4;
    sp.funcs[task->state](task);
}
