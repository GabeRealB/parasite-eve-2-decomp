#include "rooms/dryfield_night_parking_lot.h"

#include "types.h"

#include "dryfield_night_parking_lot_private.h"

#include "gameplay/area_flags.h"
#include "gameplay/area_transitions.h"
#include "gameplay/captions.h"
#include "gameplay/direction.h"
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
// The flag symbol is four bytes; the gate writes the first.
#define ROOM_EVENT_ACTIVE gRoomEventActive.eventStarted
#include "../../shared/room_events.h"

extern RoomEventActiveBytes gRoomEventActive;

/// The `GpAreaApplyRec` list the 0x11 event applies when it fires.

/// The message and request the event gate latched for its event task.
extern RoomEventMsg gRoomEventMsg;
extern RoomEventReq gRoomEventReq;

/// Set by the event gate when its last call latched a request and spawned the
/// event task; every call clears it first.

GpAreaApplyRec D_dryfield_night_parking_lot_8018155C[2] = {
    { 3, 24, 2, 0 },
    { 255, 0, 0, 0 },
};

RoomEventMsg gRoomEventMsg = { 0 };

RoomEventActiveBytes gRoomEventActive = { 0, { 63, 252, 16 } };

RoomEventReq gRoomEventReq;

static void func_dryfield_night_parking_lot_8017DBB0(Task* task);
static void func_dryfield_night_parking_lot_8017DC28(Task* task);

#include "../../shared/room_event_gate.inc.c"

#include "../../shared/room_event_task.inc.c"

/// Handler for message 0x13EE in the room's message table: the room's two
/// reports and its two events. The incoming record is first copied to `out`.
///
/// Messages 2 and 0x1D answer in `out->room` (only when `queryOnly` is clear):
/// message 2 gives nibble 0x61 plus one while nibble 0x7A is under 4, and 3
/// once it is not; message 0x1D gives 1 while nibble 0x61 is clear and 3 once
/// it is set.
///
/// Messages 0x11 and 0x12 are the events: each builds a request for the
/// room's event gate `roomEventGate` - message 0x11
/// on nibble 0x40 with collected bit 0x12, message 0x12 on nibble 0x35 with collected bit 0x10.
/// When the gate reports the event fired, 0x11 applies the area records
/// `D_dryfield_night_parking_lot_8018155C` and sets nibbles 0x46 and 0x97,
/// while 0x12 sets item-seen bit 0x110. Any other message returns 1.
s32 func_dryfield_night_parking_lot_8017D8D0(Task* task, s32 msgId, RoomEventMsg* msg, RoomEventMsg* out)
{
    RoomEventReq req;
    s32          ret;
    s32          val;
    s32          n;

    *out = *msg;
    if ((msg->areaId == 2) && (msg->queryOnly == ROOM_EVENT_EXECUTE)) {
        n = GameFlag_GetNibble(0x7A);
        if (n >= 4) {
            val = 3;
        } else {
            val = GameFlag_GetNibble(0x61) + 1;
        }
        out->room = val;
    }
    if ((msg->areaId == 0x1D) && (msg->queryOnly == ROOM_EVENT_EXECUTE)) {
        n = GameFlag_GetNibble(0x61);
        if (n == 0) {
            n = 1;
        } else {
            n = 3;
        }
        out->room = n;
    }
    if (msg->areaId == 0x11) {
        req.capCmd        = 6;
        req.missingCapCmd = 1;
        req.firstSnd      = Gp_PackStageSndId(0x520F000B);
        req.secondSnd     = Gp_PackStageSndId(0x520F0007);
        req.flagId        = 0x40;
        req.collectedBit  = 0x12;
        ret               = roomEventGate(&req, out);
        if (ret == 0) {
            ret = 2;
        }
        if (gRoomEventActive.eventStarted != 0) {
            Gp_ApplyAreaRecs(D_dryfield_night_parking_lot_8018155C);
            GameFlag_SetNibble(0x46, 1);
            GameFlag_SetNibble(0x97, 1);
        }
    } else if (msg->areaId == 0x12) {
        req.capCmd        = 3;
        req.missingCapCmd = 2;
        req.firstSnd      = Gp_PackStageSndId(0x520F000B);
        req.secondSnd     = Gp_PackStageSndId(0x520F0007);
        req.flagId        = 0x35;
        req.collectedBit  = 0x10;
        ret               = roomEventGate(&req, out);
        if (gRoomEventActive.eventStarted != 0) {
            Gp_SetItemSeenBit(0x110, 1);
        }
    } else {
        return 1;
    }
    return ret;
}

/// Handler for message 0x13F2 in the room's message table, keyed by `arg2`:
/// point 9 plays stage sound 0x520F0009 and point 10 plays 0x520F000A. Always
/// returns 0.
s32 func_dryfield_night_parking_lot_8017DAB4(Task* arg0, s32 arg1, s32 arg2, TaskMessageArg arg3)
{
    switch (arg2) {
        case 9:
            Gp_EnqueueStageSnd6(0x520F0009, 0, 0);
            break;
        case 10:
            Gp_EnqueueStageSnd6(0x520F000A, 0, 0);
            break;
    }
    return 0;
}

/// Handler for message 0x13F1 in the room's message table: does nothing and
/// returns 0.
s32 func_dryfield_night_parking_lot_8017DB04(Task* task, s32 msgId, TaskMessageArg arg2, TaskMessageArg arg3)
{
    return 0;
}

/// Handler for message 0x13F0 in the room's message table: point 4 runs CAP
/// command 4. Always returns 0.
s32 func_dryfield_night_parking_lot_8017DB0C(Task* arg0, s32 arg1, s32 arg2, TaskMessageArg arg3)
{
    if (arg2 == 4) {
        Gp_RunCapCmd1(4);
    }
    return 0;
}

/// Handler for message 0x13EF in the room's message table. When the record's
/// `actionId` is 1 on the visit whose `place` is 3, it latches nibble 0x79 once,
/// sends the player-weapon message and passes
/// `D_dryfield_night_parking_lot_8017ECB4` to `func_800E8614`. Always returns 0.
s32 func_dryfield_night_parking_lot_8017DB34(Task* task, s32 msgId, DirectionActionRequest* request, TaskMessageArg arg3)
{
    if ((request->actionId == 1) && (gGameSession->location.loc.variant == 3) && (GameFlag_GetNibble(0x79) == 0)) {
        GameFlag_SetNibble(0x79, 1);
        Gp_MsgPlayerWeapon(0);
        func_800E8614(D_dryfield_night_parking_lot_8017ECB4, 1);
    }
    return 0;
}

/// Script callback the room's script table names: stores its argument in
/// `Gp_StateF0.actor01600Wave`.
void func_dryfield_night_parking_lot_8017DBA4(s32 arg0)
{
    Gp_StateF0.actor01600Wave = arg0;
}

/// Room entry task state 0: parks the room's message table in `Task::msgTable`
/// and publishes the task in pointer slot 7. On the visit whose `place` is 3,
/// once nibble 0x79 is set - the nibble the 0x13EF handler
/// `func_dryfield_night_parking_lot_8017DB34` latches - it also sets
/// `Gp_StateF0.actor01600Wave` to 2. The state then advances.
static void func_dryfield_night_parking_lot_8017DBB0(Task* task)
{
    task->msgTable = D_dryfield_night_parking_lot_8017EC60;
    Game_SetPtrSlot(task, 7);
    if ((gGameSession->location.loc.variant == 3) && (GameFlag_GetNibble(0x79) != 0)) {
        Gp_StateF0.actor01600Wave = 2;
    }
    task->state = (s32)(task->state + 1);
}

/// Room entry task state 1: does nothing, and nothing here advances the state.
static void func_dryfield_night_parking_lot_8017DC28(Task* task)
{
}

/// The room entry task's states: set up, idle, then `taskKill`.
static const TaskFuncTable3 D_dryfield_night_parking_lot_8017D5DC = {
    { func_dryfield_night_parking_lot_8017DBB0, func_dryfield_night_parking_lot_8017DC28, taskKill },
};

/// The room entry task: copies the three-state table to the stack and runs the
/// entry the task's state selects.
void func_dryfield_night_parking_lot_8017DC30(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_night_parking_lot_8017D5DC;
    sp.funcs[task->state](task);
}
