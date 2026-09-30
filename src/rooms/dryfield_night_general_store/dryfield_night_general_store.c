#include "rooms/dryfield_night_general_store.h"

#include "types.h"

#include "dryfield_night_general_store_private.h"

#include "gameplay/captions.h"
#include "gameplay/display.h"
#include "gameplay/items.h"
#include "gameplay/message.h"
#include "gameplay/object_task.h"
#include "gameplay/world_state.h"
#include "gameplay/world_targets.h"

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

/// Descriptor of the event task `roomEventTask`.
extern TaskDesc gRoomEventTaskDesc;

/// The room's two spawnable tasks: entry 0 the CAP-command task
/// `storeToggleTask`, entry 1 the cutscene task
/// `storeCutsceneTask`.
extern TaskDesc gStoreTaskDescs[];

/// The room's message table, installed by the room task's entry state.
extern GpMsgEntry D_dryfield_night_general_store_8017E7BC[];

static void func_dryfield_night_general_store_8017DE34(Task* arg0);
static void func_dryfield_night_general_store_8017DE80(Task* task);

s32 func_dryfield_night_general_store_8017DDF0(Task*, s32, s32, s32);
s32 func_dryfield_night_general_store_8017DE24(Task*, s32, TaskMessageArg, TaskMessageArg);
s32 func_dryfield_night_general_store_8017DE2C(Task*, s32, TaskMessageArg, TaskMessageArg);

TaskDesc gRoomEventTaskDesc = { 0, 32, roomEventTask, { .model = NULL } };

TaskDesc gStoreTaskDescs[3] = {
    { 0, 32, storeToggleTask, { .model = NULL } },
    { 0, 32, storeCutsceneTask, { .model = NULL } },
    { 0xFFFF, 0, NULL, { .model = NULL } },
};

GpMsgEntry D_dryfield_night_general_store_8017E7BC[6] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, storeDoorMsg },
    { 5105, func_dryfield_night_general_store_8017DE24 },
    { 5103, func_dryfield_night_general_store_8017DE2C },
    { 5104, storeActionMsg },
    { 5106, func_dryfield_night_general_store_8017DDF0 },
    { 0x7FFFFFFF, NULL },
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

/// Message handler that plays stage sound 0x52030007 on action 7. Always
/// returns 0.
s32 func_dryfield_night_general_store_8017DDF0(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    if (arg2 == 7) {
        Gp_EnqueueStageSnd6(0x52030000 | 7, 0, 0);
    }
    return 0;
}

/// Message handler that takes no action and reports the message as not
/// handled.
s32 func_dryfield_night_general_store_8017DE24(Task* task, s32 msgId, TaskMessageArg arg2, TaskMessageArg arg3)
{
    return 0;
}

/// Message handler that takes no action and reports the message as not
/// handled.
s32 func_dryfield_night_general_store_8017DE2C(Task* task, s32 msgId, TaskMessageArg arg2, TaskMessageArg arg3)
{
    return 0;
}

/// Entry state of the room task: installs the room's message table, publishes
/// the task in pointer slot 7, advances to the idle state and raises
/// `D_80115598`.
static void func_dryfield_night_general_store_8017DE34(Task* arg0)
{
    arg0->msgTable = D_dryfield_night_general_store_8017E7BC;
    Game_SetPtrSlot(arg0, 7);
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
