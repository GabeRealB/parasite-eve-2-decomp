#include "types.h"

#include "main/task_types.h"

/* GCC orders BSS by first declaration; keep this prologue before the API headers. */
/// The cap script task the entry task spawns, the target of the scene task's
/// message 0x13EC and of the room's message 0x13F4.
Task* D_dryfield_water_tower_801876A0;

Task* D_dryfield_water_tower_801876A4;

#include "rooms/dryfield_water_tower.h"

#include "dryfield_water_tower_private.h"

#include "gameplay/captions.h"
#include "gameplay/items.h"
#include "gameplay/message.h"
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

#include "rooms/room_common.h"
#include "../../shared/room_events.h"

/* The room calls the dispatcher with only the task, leaving a1-a3 holding
   whatever the caller had, so the declaration must stay unprototyped. */

/// The saved view byte the scene task keeps while its CAP command runs, and
/// puts back when the answer is not 0xA.

/// The event the room's gate `roomEventGate` latched:
/// the incoming message and the request, kept for the event task it spawns
/// from `gRoomEventTaskDesc`, and the flag the gate sets once it
/// has done so.
extern RoomEventMsg gRoomEventMsg;
extern RoomEventReq gRoomEventReq;
extern u8           gRoomEventActive;

/// The room's message table, `(msgId, handler)` pairs ending at 0x7FFFFFFF,
/// which the entry task installs as its own `Task::msgTable`.
// Message-table callbacks use the argument views required by this TU.

static void func_dryfield_water_tower_8017DD6C(Task* arg0);
static void func_dryfield_water_tower_8017DDD0(Task* task);

static void func_dryfield_water_tower_8017DCB4(void);

RoomEventMsg gRoomEventMsg = { 0 };

u8 gRoomEventActive = 0;

u16 D_dryfield_water_tower_801876A8;

Task* D_dryfield_water_tower_801876AC;

RoomEventReq gRoomEventReq;

#include "../../shared/room_event_gate.inc.c"

#include "../../shared/room_event_task.inc.c"

/// The room entry task's three states: install the room and spawn the cap
/// script, idle, and `taskKill`.
static const TaskFuncTable3 D_dryfield_water_tower_8017D5DC = {
    { func_dryfield_water_tower_8017DD6C, func_dryfield_water_tower_8017DDD0, taskKill },
};

/// The room's scene task, spawned on script event 7. Unless nibble 0x55 has
/// reached 2 it hides the player's weapon, runs CAP command 7 and waits for it,
/// saving the view byte; a key answer of 0xA then sets nibble 0x55 to 2, sends
/// 0x13EC to the cap script and plays 0x52140009, and any other answer restores
/// the session and the view byte. With nibble 0x55 already at 2 it only runs
/// CAP command 7. Every finished path kills the task.
void func_dryfield_water_tower_8017D948(Task* arg0)
{
    switch (arg0->state) {
        case 0:
            if (GameFlag_GetNibble(0x55) < 2) {
                func_dryfield_water_tower_8017DCB4();
                Gp_MsgPlayer3F3(0);
                Gp_MsgPlayerWeapon(0);
                Gp_RunCapCmd(7, 0);
                gGameSession->eventState = 1;
                {
                    u32 view                              = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view;
                    s32 state                             = arg0->state;
                    D_dryfield_water_tower_8018768C.value = view;
                    arg0->state                           = state + 1;
                }
                return;
            }
            Gp_RunCapCmd1(7);
            break;
        case 1:
            if (Gp_CapBusy() == 0) {
                Gp_StateF0.field_4 = 2;
                /* keeps the `lw state` behind the `sb` instead of filling its load delay */
                arg0->state = arg0->state + 1;
            }
            return;
        case 2:
            if (Gp_GetCapEventKey() == 0xA) {
                GameFlag_SetNibble(0x55, 2);
                func_dryfield_water_tower_8017DCB4();
                Gp_StateF0.field_4 = 0;
                Gp_DispatchMsg(D_dryfield_water_tower_801876A0, 0x13EC, 0, 0);
                SndEvt_EnqueueType6(0x52140009, 0, 0);
            } else {
                gGameSession->eventState                                   = 0;
                gGameSession->hideHud                                      = 0;
                Gp_StateF0.field_4                                         = 0;
                gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = D_dryfield_water_tower_8018768C.value;
                Gp_MsgPlayerWeapon(1);
                Gp_MsgPlayer3F3(1);
            }
            break;
        default:
            return;
    }
    taskKill(arg0);
}

/// The room's handler for message 0x13EE, the first entry of its message table.
/// It copies the incoming record to `out` and answers by the record's first
/// halfword. For 0x13 it builds the room's event request -- flag nibble 0x34,
/// collected bit 0x10, CAP commands 0xA and 6 and two stage sounds -- and
/// hands it to the event gate with the incoming record; the gate's 0 (the
/// prerequisite missing) is answered as 2, and once the gate has latched the
/// event item 0x110 is marked seen. Any other record first drops nibble 0x55
/// from 2 back to 1 unless it is only a query. For 0x15 it also clears nibble
/// 0x4B when it reads 7, and answers 1 on stage 3 and otherwise only while
/// nibble 0x32 is 2. Everything else answers 1.
s32 func_dryfield_water_tower_8017DAF8(Task* task, s32 msgId, RoomEventMsg* msg, RoomEventMsg* out)
{
    RoomEventReq req;
    s32          ret;

    *out = *msg;
    if (msg->areaId == 0x13) {
        req.capCmd        = 0xA;
        req.missingCapCmd = 6;
        req.firstSnd      = Gp_PackStageSndId(0x5214000E);
        req.secondSnd     = Gp_PackStageSndId(0x52140003);
        req.flagId        = 0x34;
        req.collectedBit  = 0x10;
        ret               = roomEventGate(&req, msg);
        if (ret == 0) {
            ret = 2;
        }
        if (gRoomEventActive != 0) {
            Gp_SetItemSeenBit(0x110, 1);
        }
        return ret;
    }
    if (msg->queryOnly == ROOM_EVENT_EXECUTE && GameFlag_GetNibble(0x55) == 2) {
        GameFlag_SetNibble(0x55, 1);
    }
    if (msg->areaId == 0x15) {
        if (msg->queryOnly == ROOM_EVENT_EXECUTE && GameFlag_GetNibble(0x4B) == 7) {
            GameFlag_SetNibble(0x4B, 0);
        }
        if (gGameSession->location.loc.stage == 3) {
            return 1;
        }
        if (GameFlag_GetNibble(0x32) != 2) {
            return 0;
        }
    }
    return 1;
}

/// The room's handler for message 0x13F2: plays the stage sound for script
/// events 8 and 13 and answers 0 for every event.
s32 func_dryfield_water_tower_8017DC64(s32 arg0, s32 arg1, s32 arg2)
{
    switch (arg2) {
        case 8:
            Gp_EnqueueStageSnd6(0x52140008, 0, 0);
            break;
        case 13:
            Gp_EnqueueStageSnd6(0x5214000D, 0, 0);
            break;
    }
    return 0;
}

/// Shows or hides the view's sprites from nibble 0x55: modes 0 and 1 draw
/// them, 2 and 3 skip them, and any other value leaves them alone.
static void func_dryfield_water_tower_8017DCB4(void)
{
    s32 mode = GameFlag_GetNibble(0x55);

    if (mode < 0) {
        return;
    }
    if (mode < 2) {
        func_dryfield_water_tower_801802D8(1);
    } else if (mode < 4) {
        func_dryfield_water_tower_801802D8(0);
    }
}

/// The room's handler for message 0x13F1: answers 0.
s32 func_dryfield_water_tower_8017DCFC(void)
{
    return 0;
}

/// The room's handler for message 0x13F0: script event 7 spawns the scene
/// task; every event answers 0.
s32 func_dryfield_water_tower_8017DD04(Task* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    if (arg2 == 7) {
        Task_SpawnFromTable(D_dryfield_water_tower_801803D8, 0, 0, 0);
    }
    return 0;
}

/// The room's handler for message 0x13EF: answers 0.
s32 func_dryfield_water_tower_8017DD3C(void)
{
    return 0;
}

/// The room's handler for message 0x13F4: passes the message on to the cap
/// script task with the arguments it arrived with.
s32 func_dryfield_water_tower_8017DD44(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    return Gp_DispatchMsg(D_dryfield_water_tower_801876A0, msgId, arg2, arg3);
}

/// State 0 of the room entry task: installs the room's message table,
/// registers the task in game pointer slot 7 and spawns the cap script.
static void func_dryfield_water_tower_8017DD6C(Task* arg0)
{
    Task* temp_v0;

    arg0->msgTable = D_dryfield_water_tower_801803A0;
    Game_SetPtrSlot(arg0, 7);
    temp_v0                         = Task_SpawnFromTable(D_dryfield_water_tower_80182384, 0, 0, 0);
    arg0->state                     = (s32)(arg0->state + 1);
    D_dryfield_water_tower_801876A0 = temp_v0;
}

/// State 1 of the room entry task: idles.
static void func_dryfield_water_tower_8017DDD0(Task* task)
{
}

/// Runs the room entry task's current state from its three-entry table, which
/// it copies onto the stack before the call.
void func_dryfield_water_tower_8017DDD8(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_water_tower_8017D5DC;
    sp.funcs[task->state](task);
}
