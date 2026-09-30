#include "rooms/dryfield_night_motel_balcony.h"

#include "types.h"

#include "dryfield_night_motel_balcony_private.h"

#include "gameplay/area_flags.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/captions.h"
#include "gameplay/evs.h"
#include "gameplay/evs_scripts.h"
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
#include "main/task_types.h"

#include "rooms/room_common.h"
#define ROOM_EVENT_ACTIVE gRoomEventActive[0]
#include "../../shared/room_events.h"

// Preserve the following nonzero bytes with this scalar's storage.
// No separate references identify them; their role (including padding) is unresolved.
extern u8 gRoomEventActive[4];

/// A gameplay state byte; the one-shot balcony event waits while it is 1.

/// Gameplay-resident script data the room task starts: the pair handed to
/// `func_800E8634` on the first visit, and the one handed to `func_800E8614`
/// by the one-shot event.
extern s32 D_80165060;
extern s32 D_80165798;
extern u8  D_80165720;

/// The message and request the event gate latched for the event task.
extern RoomEventMsg gRoomEventMsg;
extern RoomEventReq gRoomEventReq;

/// Set by the event gate when its last call latched a request and spawned the
/// event task; every call clears it first.

GpAreaApplyRec D_dryfield_night_motel_balcony_8018F2CC[2] = {
    { 3, 29, 4, 0 },
    { 255, 0, 0, 0 },
};

RoomEventMsg gRoomEventMsg = { 0 };

u8 gRoomEventActive[4] = {
    0,
    115,
    55,
    136,
};

RoomEventReq gRoomEventReq;

static void func_dryfield_night_motel_balcony_8017DC30(Task* task);
static void func_dryfield_night_motel_balcony_8017DD0C(Task* task);

#include "../../shared/room_event_gate.inc.c"

#include "../../shared/room_event_task.inc.c"

/// The room's message handler. It copies `msg` to `out`, filling `room`
/// from game flags for messages 0x1C, 0xF and 0x1F, then routes messages 0x1C,
/// 0x1F and 0x1E through the event gate with each one's request; when the
/// gate fires, it updates the collected and seen item bits (and, for 0x1E, a
/// flag nibble and `Mc_SaveData[0].state.sceneEvent`). Any other message answers 1; a gate result
/// of 0 is reported as 2.
s32 func_dryfield_night_motel_balcony_8017D968(Task* task, s32 msgId, RoomEventMsg* msg, RoomEventMsg* out)
{
    RoomEventReq req;
    s32          flagClear;
    s32          ret;

    *out = *msg;
    if (msg->areaId == 0x1C && msg->queryOnly == ROOM_EVENT_EXECUTE) {
        out->room = GameFlag_GetNibble(0x61) + 1;
    }
    if (msg->areaId == 0xF && msg->queryOnly == ROOM_EVENT_EXECUTE) {
        out->room = GameFlag_GetNibble(0x61) + 1;
    }
    if (msg->areaId == 0x1F && msg->queryOnly == ROOM_EVENT_EXECUTE) {
        flagClear = GameFlag_GetNibble(0x96) == 0;
        out->room = flagClear ? 1 : 2;
    }
    if (msg->areaId == 0x1C) {
        req.field_0 = 7;
        req.field_4 = 4;
        req.field_8 = Gp_PackStageSndId(0x521D000A);
        req.field_C = Gp_PackStageSndId(0x521D0001);
        req.flagId  = 0x43;
        req.itemId  = 0x13;
        ret         = roomEventGate(&req, out);
        if (gRoomEventActive[0] != 0) {
            Gp_ClearCollectedBit(0x10F);
            Gp_ClearCollectedBit(0x112);
            Gp_SetItemSeenBit(0x113, 1);
        }
    } else if (msg->areaId == 0x1F) {
        req.field_0 = 5;
        req.field_4 = 2;
        req.field_8 = Gp_PackStageSndId(0x521D000A);
        req.field_C = Gp_PackStageSndId(0x521D0001);
        req.flagId  = 0x44;
        req.itemId  = 0x13;
        ret         = roomEventGate(&req, out);
        if (gRoomEventActive[0] != 0) {
            Gp_ClearCollectedBit(0x10F);
            Gp_ClearCollectedBit(0x112);
            Gp_SetItemSeenBit(0x113, 1);
        }
    } else if (msg->areaId == 0x1E) {
        req.field_0 = 6;
        req.field_4 = 3;
        req.field_8 = Gp_PackStageSndId(0x521D000A);
        req.field_C = Gp_PackStageSndId(0x521D0001);
        req.flagId  = 0x2E;
        req.itemId  = 0xF;
        ret         = roomEventGate(&req, out);
        if (gRoomEventActive[0] != 0) {
            GameFlag_SetNibble(0x30, 1);
            Mc_SaveData[0].state.sceneEvent = 3;
            func_800E3FAC(0xA2, 0xC);
        }
    } else {
        return 1;
    }
    if (ret == 0) {
        ret = 2;
    }
    return ret;
}

/// Plays stage sound 0x521D0008 or 0x521D0009 for events 8 and 9; always
/// answers 0.
s32 func_dryfield_night_motel_balcony_8017DBC8(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    switch (arg2) {
        case 0x8:
            Gp_EnqueueStageSnd6(0x521D0008, 0, 0);
            break;
        case 0x9:
            Gp_EnqueueStageSnd6(0x521D0009, 0, 0);
            break;
    }
    return 0;
}

s32 func_dryfield_night_motel_balcony_8017DC18(Task* task, s32 msgId, TaskMessageArg arg2, TaskMessageArg arg3)
{
    return 0;
}

s32 func_dryfield_night_motel_balcony_8017DC20(Task* task, s32 msgId, TaskMessageArg arg2, TaskMessageArg arg3)
{
    return 0;
}

s32 func_dryfield_night_motel_balcony_8017DC28(Task* task, s32 msgId, TaskMessageArg arg2, TaskMessageArg arg3)
{
    return 0;
}

/// Room task state 0: installs the room's message table, registers the task
/// in pointer slot 7 and reapplies the nine saved sprite-command states. On
/// place 2, room 2 with flag nibble 0x61 still clear, it also starts the
/// script pair, sets nibbles 0x61, 0x10E (arming state 1) and 0x155, clears
/// nibble 3 and sets `flowFlags` to 0x85. Then advances to the next state.
static void func_dryfield_night_motel_balcony_8017DC30(Task* task)
{
    u8 field9;

    task->msgTable = D_dryfield_night_motel_balcony_80182804;
    Game_SetPtrSlot(task, 7);
    func_dryfield_night_motel_balcony_8017E3C8();
    field9 = gGameSession->location.loc.variant;
    if (field9 == 2 && gGameSession->location.loc.room == field9 && GameFlag_GetNibble(0x61) == 0) {
        func_800E8634(&D_80165060, 0, &D_80165798);
        GameFlag_SetNibble(0x61, 1);
        GameFlag_SetNibble(0x10E, 1);
        GameFlag_SetNibble(3, 0);
        GameFlag_SetNibble(0x155, 1);
        gGameSession->flowFlags = (GAME_SESSION_FLOW_SKIP_ENDING_MUSIC | GAME_SESSION_FLOW_LOAD_ENDING_MUSIC_ONLY | GAME_SESSION_FLOW_REEQUIP_WEAPON);
    }
    task->state = task->state + 1;
}

/// Room task state 1: once the event state is idle, `Gp_StateC08.field_A` is not 1 and
/// flag nibble 0x10E is 1, runs the one-shot script and moves the nibble to 2.
static void func_dryfield_night_motel_balcony_8017DD0C(Task* task)
{
    if (gGameSession->eventState == 0 && Gp_StateC08.field_A != 1 && GameFlag_GetNibble(0x10E) == 1) {
        func_800E8614(&D_80165720, 0);
        GameFlag_SetNibble(0x10E, 2);
    }
}

/// The room task's three states: setup, the per-tick balcony event check,
/// and exit.
static const TaskFuncTable3 D_dryfield_night_motel_balcony_8017D5DC = {
    func_dryfield_night_motel_balcony_8017DC30,
    func_dryfield_night_motel_balcony_8017DD0C,
    taskKill,
};

/// Runs the room task's current state from its state table, dispatching
/// through a copy of the table taken onto the stack.
void func_dryfield_night_motel_balcony_8017DD78(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_night_motel_balcony_8017D5DC;
    sp.funcs[task->state](task);
}
