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

/// Descriptor of the event task `roomEventTask`.
extern TaskDesc gRoomEventTaskDesc;

/// The room's two spawnable tasks: entry 0 the CAP-command task
/// `func_dryfield_night_general_store_8017DCA8`, entry 1 the cutscene task
/// `func_dryfield_night_general_store_8017DAF0`.
extern TaskDesc D_dryfield_night_general_store_8017E798[];

/// The room's message table, installed by the room task's entry state.
extern GpMsgEntry D_dryfield_night_general_store_8017E7BC[];

static void func_dryfield_night_general_store_8017DE34(Task* arg0);
static void func_dryfield_night_general_store_8017DE80(Task* task);

s32  func_dryfield_night_general_store_8017D904(Task*, s32, RoomEventMsg*, RoomEventMsg*);
void func_dryfield_night_general_store_8017DAF0(Task*);
void func_dryfield_night_general_store_8017DCA8(Task*);
s32  func_dryfield_night_general_store_8017DD88(Task*, s32, s32, TaskMessageArg);
s32  func_dryfield_night_general_store_8017DDF0(Task*, s32, s32, s32);
s32  func_dryfield_night_general_store_8017DE24(Task*, s32, TaskMessageArg, TaskMessageArg);
s32  func_dryfield_night_general_store_8017DE2C(Task*, s32, TaskMessageArg, TaskMessageArg);

TaskDesc gRoomEventTaskDesc = { 0, 32, roomEventTask, { .model = NULL } };

TaskDesc D_dryfield_night_general_store_8017E798[3] = {
    { 0, 32, func_dryfield_night_general_store_8017DCA8, { .model = NULL } },
    { 0, 32, func_dryfield_night_general_store_8017DAF0, { .model = NULL } },
    { 0xFFFF, 0, NULL, { .model = NULL } },
};

GpMsgEntry D_dryfield_night_general_store_8017E7BC[6] = {
    { ROOM_EVENT_MESSAGE_RESOLVE, func_dryfield_night_general_store_8017D904 },
    { 5105, func_dryfield_night_general_store_8017DE24 },
    { 5103, func_dryfield_night_general_store_8017DE2C },
    { 5104, func_dryfield_night_general_store_8017DD88 },
    { 5106, func_dryfield_night_general_store_8017DDF0 },
    { 0x7FFFFFFF, NULL },
};

#include "../../shared/room_event_gate.inc.c"

#include "../../shared/room_event_task.inc.c"

/// Handler for the store's two event ids. Both answer with a furniture-style
/// "which variant" byte in `out->room`, and a non-zero `queryOnly` asks what
/// would happen without the side effects.
///
/// Message 1 is the grandfather clock: with nibble 0x63 clear the reply is the
/// id itself, otherwise 4, or 2 + nibble 0x61 while nibble 0x7A is still below
/// 4. The final arm offers the gate a request that plays the two stage sounds
/// 0x5203000C / 0x52030003 under flag nibble 0x3B.
///
/// Message 0x26 is the shop till: with nibble 0xC9 set the reply is 2, or 1
/// while nibble 0x53 is clear, plus 2 more while nibble 0x51 is clear;
/// otherwise 5, or 6 while nibble 0x51 is clear. The arm that is not asking
/// latches `warp` / `room` for the spawned task and answers 2, or runs
/// CAP command 0xE when nibble 0x62 is set. Anything else answers 1.
s32 func_dryfield_night_general_store_8017D904(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    RoomEventReq req;
    u16          msgId;
    s32          v;

    *out  = *in;
    msgId = in->areaId;
    if (msgId == 1 && in->queryOnly == ROOM_EVENT_EXECUTE) {
        if (GameFlag_GetNibble(0x63) == 0) {
            out->room = msgId;
        } else {
            if (GameFlag_GetNibble(0x7A) >= 4) {
                v = 4;
            } else {
                v = GameFlag_GetNibble(0x61) + 2;
            }
            out->room = v;
        }
    }
    if (in->areaId == 0x26 && in->queryOnly == ROOM_EVENT_EXECUTE) {
        if (GameFlag_GetNibble(0xC9) != 0) {
            if (GameFlag_GetNibble(0x53) == 0) {
                out->room = 1;
            } else {
                out->room = 2;
            }
            if (GameFlag_GetNibble(0x51) == 0) {
                out->room = out->room + 2;
            }
        } else if (GameFlag_GetNibble(0x51) == 0) {
            out->room = 6;
        } else {
            out->room = 5;
        }
    }
    if (in->areaId == 1) {
        req.capCmd        = 0xD;
        req.missingCapCmd = 0xD;
        req.firstSnd      = Gp_PackStageSndId(0x5203000C);
        req.secondSnd     = Gp_PackStageSndId(0x52030003);
        req.flagId        = 0x3B;
        req.collectedBit  = 0;
        return roomEventGate(&req, in);
    }
    if (in->areaId != 0x26) {
        return 1;
    }
    if (in->queryOnly != ROOM_EVENT_EXECUTE) {
        return 2;
    }
    if (GameFlag_GetNibble(0x62) == 0) {
        Task_SpawnFromTable(D_dryfield_night_general_store_8017E798, 1, 0, 0);
        D_dryfield_night_general_store_801858C5 = in->warp;
        D_dryfield_night_general_store_801858C6 = in->room;
    } else {
        Gp_RunCapCmd1(0xE);
    }
    return 2;
}

/// The room's cutscene task, a six-state script. State 0 silences the
/// player's weapon messages, saves the stage byte `Mc_SaveData[0].state.location.loc.view`
/// and forces it to 0x10; states 1 and 3 each let one frame pass. State 2
/// queues stage sound 0x5203000D, runs CAP command 0xF and raises
/// `Gp_StateF0.field_4` / `D_80115690`.
///
/// State 4 checks the CAP event key: 0xB spawns helper task 0x31 with a
/// `ScreenFade` whose `rampFrames` is 8 and moves on; any other key ends the
/// cutscene - `Gp_StateF0.field_4` cleared, stage sound 0x5203000E, the saved stage
/// byte written to `Mc_SaveData[0].state.location.loc.view` and the weapon messages re-enabled. State 5
/// queues sound event 0x80000000, points the save's location at area 0x26
/// with the latched warp point and room, raises `gDisplayState.spriteVariant` and spawns
/// helper task 0x11. Both finishing arms kill the task.
void func_dryfield_night_general_store_8017DAF0(Task* arg0)
{
    switch (arg0->state) {
        case 0:
            Gp_MsgPlayerWeapon(0);
            Gp_MsgPlayer3F3(0);
            D_dryfield_night_general_store_801858B4 = Mc_SaveData[0].state.location.loc.view;
            Mc_SaveData[0].state.location.loc.view  = 0x10;
            arg0->state                            += 1;
            return;
        case 1:
        case 3:
            arg0->state += 1;
            return;
        case 2:
            Gp_EnqueueStageSnd6(0x5203000D, 0, 0);
            Gp_StateF0.field_4 = 1;
            Gp_RunCapCmd1(0xF);
            D_80115690   = 1;
            arg0->state += 1;
            return;
        case 4:
            if (Gp_GetCapEventKey() == 0xB) {
                D_dryfield_night_general_store_801858B8.blend      = SCREEN_FADE_SUBTRACT;
                D_dryfield_night_general_store_801858B8.phase      = SCREEN_FADE_RUNNING;
                D_dryfield_night_general_store_801858B8.rampFrames = 8;
                Task_Spawn(1, 0x31, 0, &D_dryfield_night_general_store_801858B8);
                arg0->state += 1;
                return;
            }
            Gp_StateF0.field_4 = 0;
            Gp_EnqueueStageSnd6(0x5203000E, 0, 0);
            Mc_SaveData[0].state.location.loc.view = D_dryfield_night_general_store_801858B4;
            Gp_MsgPlayerWeapon(1);
            Gp_MsgPlayer3F3(1);
            break;
        case 5:
            SndEvt_EnqueueType7(0x80000000, 0);
            Mc_SaveData[0].state.location.loc.area = 0x26;
            Mc_SaveData[0].state.location.loc.warp = D_dryfield_night_general_store_801858C5;
            Mc_SaveData[0].state.location.loc.room = D_dryfield_night_general_store_801858C6;
            gDisplayState.spriteVariant            = 1;
            Task_Spawn(0, 0x11, 0, 0);
            break;
        default:
            return;
    }
    taskKill(arg0);
}

/// The room task's three-state table, run from a stack copy by
/// `func_dryfield_night_general_store_8017DE88`: the entry state
/// `func_dryfield_night_general_store_8017DE34`, the idle state
/// `func_dryfield_night_general_store_8017DE80`, then `taskKill`.
static const TaskFuncTable3 D_dryfield_night_general_store_8017D5F4 = {
    { func_dryfield_night_general_store_8017DE34, func_dryfield_night_general_store_8017DE80, taskKill },
};

/// A task that runs cap command `spawnArg2` and waits for it to finish; if the
/// cap then reports an event key of 0xA or above, it toggles game-flag nibble
/// `spawnArg1` between 0 and 1. The task then kills itself.
void func_dryfield_night_general_store_8017DCA8(Task* task)
{
    s32 flag;
    s32 cmd;

    flag = task->spawnArg1.value;
    cmd  = task->spawnArg2.value;
    switch (task->state) {
        case 0:
            Gp_RunCapCmd1(cmd);
            goto advance;
        case 1:
            if (Gp_CapBusy() != 0) {
                break;
            }
            goto advance;
        case 2:
            if (Gp_GetCapEventKey() >= 0xA) {
                GameFlag_SetNibble(flag, GameFlag_GetNibble(flag) == 0);
            }
        advance:
            task->state = task->state + 1;
            break;
        case 3:
            taskKill(task);
            break;
    }
}

/// Message handler for actions 0x18 and 9. Action 0x18 hands
/// `Gp_SpawnIfCapIdle` 0x18 when pointer slot 0xA holds a task and 0x19
/// otherwise; action 9 spawns the room's CAP-command task to run CAP command 9
/// and toggle flag nibble 0x53. Always returns 0.
s32 func_dryfield_night_general_store_8017DD88(Task* arg0, s32 arg1, s32 arg2, TaskMessageArg arg3)
{
    s32   arg;
    void* slot;

    if (arg2 == 0x18) {
        slot = gameGetPtrSlot(0xA);
        arg  = 0x19;
        if (slot != 0) {
            arg = 0x18;
        }
        Gp_SpawnIfCapIdle(arg, 0);
    }
    if (arg2 == 9) {
        Task_SpawnFromTable(D_dryfield_night_general_store_8017E798, 0, 0x53, 9);
    }
    return 0;
}

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
