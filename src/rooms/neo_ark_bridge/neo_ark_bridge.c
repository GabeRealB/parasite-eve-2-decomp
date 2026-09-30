#include "rooms/neo_ark_bridge.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/rand.h>

#include "gte.h"
#include "types.h"

#include "gameplay/direction.h"
#include "gameplay/message.h"
#include "gameplay/world_state.h"
#include "gameplay/world_targets.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/fs.h"
#include "main/gfx.h"
#include "main/gfxgte.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"

#include "mapui/map_neo_ark.h"

#include "overlay.h"
#include "../../shared/water_effects.h"

s32     rcos(s32);
s32     rsin(s32);
MATRIX* TransposeMatrix(MATRIX*, MATRIX*);

/// The room's message table.
extern GpMsgEntry D_neo_ark_bridge_80181F30[];

s32 func_neo_ark_bridge_8017E82C(Task*, s32, TaskMessageArg, TaskMessageArg);
s32 func_neo_ark_bridge_8017E834(Task*, s32, RoomEventMsg*, RoomEventMsg*);
s32 func_neo_ark_bridge_8017E878(Task*, s32, TaskMessageArg, TaskMessageArg);
s32 func_neo_ark_bridge_8017E880(Task*, s32, TaskMessageArg, TaskMessageArg);

TaskDesc D_neo_ark_bridge_80181F18 = { 0, 192, waterRefractionTask, { .model = NULL } };

TaskDesc D_neo_ark_bridge_80181F24 = { 0, 192, waterDistortBandTask, { .model = NULL } };

GpMsgEntry D_neo_ark_bridge_80181F30[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, func_neo_ark_bridge_8017E834 },
    { 5105, func_neo_ark_bridge_8017E82C },
    { 5103, func_neo_ark_bridge_8017E880 },
    { 5104, func_neo_ark_bridge_8017E878 },
    { 0x7FFFFFFF, NULL },
};

static void func_neo_ark_bridge_8017E888(Task* arg0);
static void func_neo_ark_bridge_8017E8F4(Task* task);

#include "../../shared/water_refraction_task.inc.c"

#include "../../shared/water_distort_band_task.inc.c"

s32 func_neo_ark_bridge_8017E82C(Task* task, s32 msgId, TaskMessageArg arg2, TaskMessageArg arg3)
{
    return 0;
}

/// Room message handler for the bridge's save location: copies the incoming
/// record onto the outgoing one and forwards both to `func_map_neo_ark_80179B14`. Always
/// answers 1.
s32 func_neo_ark_bridge_8017E834(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    *out = *in;
    func_map_neo_ark_80179B14(in, out);
    return 1;
}

s32 func_neo_ark_bridge_8017E878(Task* task, s32 msgId, TaskMessageArg arg2, TaskMessageArg arg3)
{
    return 0;
}

s32 func_neo_ark_bridge_8017E880(Task* task, s32 msgId, TaskMessageArg arg2, TaskMessageArg arg3)
{
    return 0;
}

/// Room entry task tick: installs the room's message table (ids `0x13EE`-`0x13F1`),
/// hands the task to pointer slot 7, queues sound events `0x551B0003` and
/// `0x551B0004`, then advances state.
static void func_neo_ark_bridge_8017E888(Task* arg0)
{
    arg0->msgTable = D_neo_ark_bridge_80181F30;
    Game_SetPtrSlot(arg0, 7);
    SndEvt_EnqueueType6(0x551B0003, 0, 0);
    SndEvt_EnqueueType6(0x551B0004, 0, 0);
    arg0->state = (s32)(arg0->state + 1);
}

static void func_neo_ark_bridge_8017E8F4(Task* task)
{
}

/// State handlers of the room's entry task, indexed by its state through
/// `func_neo_ark_bridge_8017E8FC`: set-up, idle, then kill.
static const TaskFuncTable3 D_neo_ark_bridge_8017D614 = {
    { func_neo_ark_bridge_8017E888, func_neo_ark_bridge_8017E8F4, taskKill }
};

/// Task tick that dispatches on the task's state through the three-entry
/// handler table `D_neo_ark_bridge_8017D614`, copied to the stack first.
void func_neo_ark_bridge_8017E8FC(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_neo_ark_bridge_8017D614;
    sp.funcs[task->state](task);
}
