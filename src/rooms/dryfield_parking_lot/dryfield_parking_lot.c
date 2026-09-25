#include "common.h"

#include "gameplay/268.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "rooms/room_common.h"

/// The `GpAreaApplyRec` list the 0x11 answer applies when the event fires.
/// The address sits past the end of this package, so the linker resolves it
/// from its auto-generated undefined-symbols file as an absolute one rather
/// than as room data.
extern GpAreaApplyRec D_8018155C[];

/// Descriptor of the event task the event gate spawns.
extern TaskDesc D_dryfield_parking_lot_8017DBF8;

/// The room's message table, published in `Task::msgTable` by the entry task
/// (ids 0x13EE-0x13F2).
extern GpMsgEntry D_dryfield_parking_lot_8017DC04[];

/// Per-view values `func_dryfield_parking_lot_8017DBAC` publishes, indexed by
/// camera view index minus one.
extern u16 D_dryfield_parking_lot_8017DC34[];

/// The message and request the event gate latched for its event task.
extern RoomEventMsg D_dryfield_parking_lot_8017FB50;
extern RoomEventReq D_dryfield_parking_lot_8017FB5C;

/// Set by the event gate when its last call latched a request and spawned the
/// event task; every call clears it first.
extern u8 D_dryfield_parking_lot_8017FB58;

/// The room's event gate. A request whose flag nibble is already set (or clear,
/// for a negative `flagId`) answers 1. One whose prerequisite item is missing
/// runs the request's CAP command `field_4` and answers 0. Otherwise the
/// message and request are latched, the nibble is written, the event task is
/// spawned and the answer is 2. A non-zero `field_5` on the message only
/// reports the answer, with none of the side effects.
s32 func_dryfield_parking_lot_8017D5E8(RoomEventReq* req, RoomEventMsg* msg)
{
    s32 flag;
    s32 id;
    s32 mode;
    s32 got;
    s32 ret;
    s32 neg;

    flag                            = req->flagId;
    D_dryfield_parking_lot_8017FB58 = 0;
    neg                             = flag < 0;
    got                             = (s16)flag;
    if (neg) {
        flag = -flag;
        got  = GameFlag_GetNibble(flag) == 0;
    } else {
        got = GameFlag_GetNibble(got);
    }
    ret = 1;
    if (got == 0) {
        if (Gp_HasCollectedBit(req->itemId) != 0 || req->itemId == 0) {
            ret = 2;
            if (msg->field_5 == 0) {
                D_dryfield_parking_lot_8017FB50 = *msg;
                D_dryfield_parking_lot_8017FB5C = *req;
                id                              = req->flagId;
                mode                            = 1;
                if (id < 0) {
                    id   = -id;
                    mode = 0;
                }
                GameFlag_SetNibble(id, mode);
                Task_SpawnFromTable(&D_dryfield_parking_lot_8017DBF8, 0, 0, 0);
                D_dryfield_parking_lot_8017FB58 = 1;
                return 2;
            }
            return ret;
        }
        ret = 0;
        if (msg->field_5 == 0) {
            Gp_RunCapCmd1(req->field_4);
            Gp_SetNibbleIf(msg->field_6, 2);
            ret = 0;
        }
        return ret;
    }
    return ret;
}

/// The event task the gate spawns. State 0 runs the latched request's CAP
/// command; states 1-4 play its two sounds in turn (`field_8`, then
/// `field_C`), each skipped when zero and waited on until its voice falls
/// silent; state 5 writes the latched message's destination into the save
/// data and hands over to task type 0x11.
void func_dryfield_parking_lot_8017D74C(Task* task)
{
    switch (task->state) {
        case 0:
            Gp_StateF0.field_4 = 1;
            Gp_MsgPlayerWeapon(0);
            Gp_RunCapCmd1(D_dryfield_parking_lot_8017FB5C.field_0);
            if (D_dryfield_parking_lot_8017FB5C.field_8 != 0) {
                SndEvt_EnqueueType6(D_dryfield_parking_lot_8017FB5C.field_8, 0, 0);
                task->state++;
            } else {
                task->state = 2;
            }
            break;
        case 1:
            if (SndVoice_HasActiveId(D_dryfield_parking_lot_8017FB5C.field_8) == 0) {
                task->state++;
            }
            break;
        case 2:
            task->state++;
            break;
        case 3:
            if (D_dryfield_parking_lot_8017FB5C.field_C != 0) {
                SndEvt_EnqueueType6(D_dryfield_parking_lot_8017FB5C.field_C, 0, 0);
                task->state++;
            } else {
                task->state = 5;
            }
            break;
        case 4:
            if (SndVoice_HasActiveId(D_dryfield_parking_lot_8017FB5C.field_C) == 0) {
                task->state++;
            }
            break;
        case 5:
            SndEvt_EnqueueType7(0x80000000, 0);
            gDisplayState.roomVariant = 1;
            Mc_SaveData.at4.loc.area  = D_dryfield_parking_lot_8017FB50.msgId;
            Mc_SaveData.at4.loc.warp  = D_dryfield_parking_lot_8017FB50.field_2;
            Mc_SaveData.at4.loc.room  = (u8)D_dryfield_parking_lot_8017FB50.field_3;
            Task_Spawn(0, 0x11, 0, 0);
            taskKill(task);
            break;
    }
}

/// Handler for message 0x13EE in the room's message table: the room's two
/// reports and its two events. The incoming record is first copied to `out`.
///
/// Messages 2 and 0x1D answer in `out->field_3` (only when `field_5` is clear):
/// message 2 gives nibble 0x61 plus one while nibble 0x7A is under 4, and 3
/// once it is not; message 0x1D gives 1 while nibble 0x61 is clear and 3 once
/// it is set.
///
/// Messages 0x11 and 0x12 are the events: each builds a request for the
/// room's event gate `func_dryfield_parking_lot_8017D5E8` - message 0x11 on
/// nibble 0x40 with item 0x12, message 0x12 on nibble 0x35 with item 0x10.
/// When the gate reports the event fired, 0x11 applies the area records
/// `D_8018155C` and sets nibbles 0x46 and 0x97, while 0x12 sets item-seen bit
/// 0x110. Any other message returns 1.
s32 func_dryfield_parking_lot_8017D8BC(Task* task, s32 msgId, RoomEventMsg* msg, RoomEventMsg* out)
{
    RoomEventReq req;
    s32          ret;
    s32          val;
    s32          n;

    *out = *msg;
    if ((msg->msgId == 2) && (msg->field_5 == 0)) {
        n = GameFlag_GetNibble(0x7A);
        if (n < 4) {
            val = 3;
            TOUCH_REG(val);
            val = GameFlag_GetNibble(0x61) + 1;
        } else {
            val = 3;
        }
        out->field_3 = val;
    }
    if ((msg->msgId == 0x1D) && (msg->field_5 == 0)) {
        n = GameFlag_GetNibble(0x61);
        if (n == 0) {
            n = 1;
        } else {
            n = 3;
        }
        out->field_3 = n;
    }
    if (msg->msgId == 0x11) {
        req.field_0 = 6;
        req.field_4 = 1;
        req.field_8 = Gp_PackStageSndId(0x520F000B);
        req.field_C = Gp_PackStageSndId(0x520F0007);
        req.flagId  = 0x40;
        req.itemId  = 0x12;
        ret         = func_dryfield_parking_lot_8017D5E8(&req, out);
        if (ret == 0) {
            ret = 2;
        }
        if (D_dryfield_parking_lot_8017FB58 != 0) {
            Gp_ApplyAreaRecs(D_8018155C);
            GameFlag_SetNibble(0x46, 1);
            GameFlag_SetNibble(0x97, 1);
        }
    } else if (msg->msgId == 0x12) {
        req.field_0 = 3;
        req.field_4 = 2;
        req.field_8 = Gp_PackStageSndId(0x520F000B);
        req.field_C = Gp_PackStageSndId(0x520F0007);
        req.flagId  = 0x35;
        req.itemId  = 0x10;
        ret         = func_dryfield_parking_lot_8017D5E8(&req, out);
        if (D_dryfield_parking_lot_8017FB58 != 0) {
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
s32 func_dryfield_parking_lot_8017DAA0(s32 arg0, s32 arg1, s32 arg2)
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
s32 func_dryfield_parking_lot_8017DAF0(void)
{
    return 0;
}

/// Handler for message 0x13F0 in the room's message table: does nothing and
/// returns 0.
s32 func_dryfield_parking_lot_8017DAF8(void)
{
    return 0;
}

/// Handler for message 0x13EF in the room's message table: does nothing and
/// returns 0.
s32 func_dryfield_parking_lot_8017DB00(void)
{
    return 0;
}

/// Room entry task state 0: parks the room's message table in `Task::msgTable`,
/// publishes the task in pointer slot 7 and advances the state.
void func_dryfield_parking_lot_8017DB08(Task* task)
{
    task->msgTable = D_dryfield_parking_lot_8017DC04;
    Game_SetPtrSlot(task, 7);
    task->state = (s32)(task->state + 1);
}

/// Room entry task state 1: does nothing, and nothing here advances the state.
void func_dryfield_parking_lot_8017DB4C(Task* task)
{
}

/// The room entry task's states: set up, idle, then `taskKill`.
const TaskFuncTable3 D_dryfield_parking_lot_8017D5DC = {
    { func_dryfield_parking_lot_8017DB08, func_dryfield_parking_lot_8017DB4C, taskKill },
};

/// The room entry task: copies the three-state table to the stack and runs the
/// entry the task's state selects.
void func_dryfield_parking_lot_8017DB54(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_parking_lot_8017D5DC;
    sp.funcs[task->state](task);
}

/// Publishes the value the current camera view maps to: stores
/// `D_dryfield_parking_lot_8017DC34[view - 1]` into `Gp_State1C`'s
/// `roomEffectMode`. Nothing in the room calls it; gameplay's data holds its
/// address.
void func_dryfield_parking_lot_8017DBAC(void)
{
    Gp_State1C->roomEffectMode = D_dryfield_parking_lot_8017DC34[(Gp_GetViewIndex() & 0xFF) - 1];
}
