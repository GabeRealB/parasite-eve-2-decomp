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
#include "gameplay/scene_combat.h"

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
extern TaskMessageEntry D_neo_ark_bridge_80181F30[];

s32 func_neo_ark_bridge_8017E82C(Task*, s32, s32, s32);
s32 func_neo_ark_bridge_8017E834(Task*, s32, RoomEventMsg*, RoomEventMsg*);
s32 func_neo_ark_bridge_8017E878(Task*, s32, s32, s32);
s32 func_neo_ark_bridge_8017E880(Task*, s32, s32, s32);

TaskDesc D_neo_ark_bridge_80181F18 = { { { TASK_BODY_NONE, 192 } }, waterRefractionTask, { .value = 0 } };

TaskDesc D_neo_ark_bridge_80181F24 = { { { TASK_BODY_NONE, 192 } }, waterDistortBandTask, { .value = 0 } };

TaskMessageEntry D_neo_ark_bridge_80181F30[5] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, func_neo_ark_bridge_8017E834 },
    { 5105, func_neo_ark_bridge_8017E82C },
    { DIRECTION_MESSAGE_ROOM_ACTION, func_neo_ark_bridge_8017E880 },
    { ROOM_MESSAGE_COMMAND, func_neo_ark_bridge_8017E878 },
    { TASK_MESSAGE_TABLE_END, NULL },
};

static void func_neo_ark_bridge_8017E888(Task* arg0);
static void func_neo_ark_bridge_8017E8F4(Task* task);

#include "../../shared/water_refraction_task.inc.c"

#include "../../shared/water_distort_band_task.inc.c"

s32 func_neo_ark_bridge_8017E82C(Task* task, s32 msgId, s32 arg2, s32 arg3)
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

s32 func_neo_ark_bridge_8017E878(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}

s32 func_neo_ark_bridge_8017E880(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return 0;
}

/// Room entry task tick: installs the room's message table (ids `0x13EE`-`0x13F1`),
/// hands the task to pointer slot 7, queues sound events `0x551B0003` and
/// `0x551B0004`, then advances state.
static void func_neo_ark_bridge_8017E888(Task* arg0)
{
    arg0->msgTable = D_neo_ark_bridge_80181F30;
    gameSetTaskSlot(arg0, GAME_TASK_SLOT_ROOM);
    sndEvtRequestScriptStart(SOUND_NEO_ARK_BRIDGE_AMBIENCE_1, 0, 0);
    sndEvtRequestScriptStart(SOUND_NEO_ARK_BRIDGE_AMBIENCE_2, 0, 0);
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
