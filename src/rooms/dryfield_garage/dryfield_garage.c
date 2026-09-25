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

extern s16      D_80071076;
extern TaskDesc D_80141B6C[];

/// The event message and request the gate latched for the event task, and the
/// flag saying one was latched this call.
extern RoomEventMsg D_dryfield_garage_80180224;
extern RoomEventReq D_dryfield_garage_80180230;
extern u8           D_dryfield_garage_8018022C;

/// Descriptor of the event task `func_dryfield_garage_8017D74C`.
extern TaskDesc D_dryfield_garage_8017DC70;

/// The room's message table, installed on the room task by its entry state.
extern GpMsgEntry D_dryfield_garage_8017DC7C[];

/// Spawn table of the task `func_dryfield_garage_8017DAA0`, ended by a 0xFFFF
/// entry.
extern TaskDesc D_dryfield_garage_8017DCAC[];

extern s32     D_dryfield_garage_8017DCC4;
extern GpObj4A D_dryfield_garage_8017FD1C;
extern Task*   D_dryfield_garage_8018021C;

void func_dryfield_garage_8017D74C(Task* task);
void func_dryfield_garage_8017DB18(Task* arg0);
void func_dryfield_garage_8017DC08(Task* task);

/// Event gate for a room exit. Returns 1 when game-flag nibble `req->flagId`
/// already reads set (clear, for a negative id). Otherwise, when
/// `req->itemId` has been collected or is 0, it returns 2 and - unless
/// `msg->field_5` asks for a dry run - latches `msg` and `req`, sets the
/// nibble and spawns the event task. When the item is missing it returns 0
/// and, outside a dry run, runs cap command `req->field_4`. Nothing in this
/// room calls it.
s32 func_dryfield_garage_8017D5E8(RoomEventReq* req, RoomEventMsg* msg)
{
    s32 flag;
    s32 id;
    s32 mode;
    s32 got;
    s32 ret;
    s32 neg;

    flag                       = req->flagId;
    D_dryfield_garage_8018022C = 0;
    neg                        = flag < 0;
    got                        = (s16)flag;
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
                D_dryfield_garage_80180224 = *msg;
                D_dryfield_garage_80180230 = *req;
                id                         = req->flagId;
                mode                       = 1;
                if (id < 0) {
                    id   = -id;
                    mode = 0;
                }
                GameFlag_SetNibble(id, mode);
                Task_SpawnFromTable(&D_dryfield_garage_8017DC70, 0, 0, 0);
                D_dryfield_garage_8018022C = 1;
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

/// The event task the gate spawns: runs the latched request's cap command,
/// plays its two sound ids in turn, each waited out, then warps to the area,
/// warp point and room the latched message names.
void func_dryfield_garage_8017D74C(Task* task)
{
    switch (task->state) {
        case 0:
            Gp_StateF0.field_4 = 1;
            Gp_MsgPlayerWeapon(0);
            Gp_RunCapCmd1(D_dryfield_garage_80180230.field_0);
            if (D_dryfield_garage_80180230.field_8 != 0) {
                SndEvt_EnqueueType6(D_dryfield_garage_80180230.field_8, 0, 0);
                task->state++;
            } else {
                task->state = 2;
            }
            break;
        case 1:
            if (SndVoice_HasActiveId(D_dryfield_garage_80180230.field_8) == 0) {
                task->state++;
            }
            break;
        case 2:
            task->state++;
            break;
        case 3:
            if (D_dryfield_garage_80180230.field_C != 0) {
                SndEvt_EnqueueType6(D_dryfield_garage_80180230.field_C, 0, 0);
                task->state++;
            } else {
                task->state = 5;
            }
            break;
        case 4:
            if (SndVoice_HasActiveId(D_dryfield_garage_80180230.field_C) == 0) {
                task->state++;
            }
            break;
        case 5:
            SndEvt_EnqueueType7(0x80000000, 0);
            D_80071076               = 1;
            Mc_SaveData.at4.loc.area = D_dryfield_garage_80180224.msgId;
            Mc_SaveData.at4.loc.warp = D_dryfield_garage_80180224.field_2;
            Mc_SaveData.at4.loc.room = (u8)D_dryfield_garage_80180224.field_3;
            Task_Spawn(0, 0x11, 0, 0);
            taskKill(task);
            break;
    }
}

/// The room task's three-state table, run from a stack copy by
/// `func_dryfield_garage_8017DC10`: the entry state
/// `func_dryfield_garage_8017DB18`, the idle state
/// `func_dryfield_garage_8017DC08`, then `taskKill`.
const TaskFuncTable3 D_dryfield_garage_8017D5DC = {
    { func_dryfield_garage_8017DB18, func_dryfield_garage_8017DC08, taskKill },
};

/// Handler for message 0x13F2 in the room's message table: on event 9 it plays
/// stage sound 0x52030009, on event 0x6C it reads the cap event key, and it
/// always reports the message as not handled.
s32 func_dryfield_garage_8017D8BC(s32 arg0, s32 arg1, s32 arg2)
{
    switch (arg2) {
        case 0x9:
            Gp_EnqueueStageSnd6(0x52030009, 0, 0);
            break;
        case 0x6C:
            Gp_GetCapEventKey();
            break;
    }
    return 0;
}

/// Handler for message 0x13F1 in the room's message table: the room takes no
/// action and reports the message as not handled.
s32 func_dryfield_garage_8017D914(void)
{
    return 0;
}

/// Handler for message 0x13EE in the room's message table, which filters a
/// warp request: copies `in` to `out`. For area 0x1A it answers 2 while nibble
/// 0x33 is clear, spawning the task of `D_dryfield_garage_8017DCAC` outside a
/// dry run; once 0x33 is set it sets nibble 0x2F (and 0x4B to 3) the first
/// time. For area 0x17 it reports nibble 0x47 in `out->field_3`, 1 when clear
/// and 2 when set. Otherwise it answers 1.
s32 func_dryfield_garage_8017D91C(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    // Never touched, but its stack slot is load-bearing: `expand_decl` gives
    // every BLKmode local a frame slot whether or not anything reads it, and
    // MIPS_STACK_ALIGN(0x14) is what puts the saves at 0x28 and the frame at
    // 0x38. Dropping it shrinks the frame to 0x20 and the overlay stops
    // matching. See DECOMPILATION_LEARNINGS.md, "A frame 24 bytes too small is
    // a dead aggregate local".
    RoomEventReq req;
    s32          nib;

    *out = *in;
    if (in->msgId == 0x1A) {
        if (GameFlag_GetNibble(0x33) == 0) {
            if (in->field_5 == 0) {
                Task_SpawnFromTable(D_dryfield_garage_8017DCAC, 0, 0, 0);
            }
            return 2;
        }
        if (GameFlag_GetNibble(0x2F) == 0) {
            GameFlag_SetNibble(0x2F, 1);
            GameFlag_SetNibble(0x4B, 3);
        }
    }
    if (in->msgId == 0x17) {
        if (in->field_5 == 0) {
            nib = GameFlag_GetNibble(0x47);
            if (nib == 0) {
                nib = 1;
            } else {
                nib = 2;
            }
            out->field_3 = nib;
        }
    }
    return 1;
}

/// Handler for message 0x13F0 in the room's message table: on event 0x10 it
/// runs cap command 0x16 if nibble 0xFD is set, else 0x10. Always answers 0.
s32 func_dryfield_garage_8017DA18(s32 arg0, s32 arg1, s32 arg2)
{
    if (arg2 == 0x10) {
        Gp_RunCapCmd1(GameFlag_GetNibble(0xFD) != 0 ? 0x16 : 0x10);
    }
    return 0;
}

/// Handler for message 0x13EF in the room's message table: for warp point 2
/// outside place 1 it calls `Gp_SpawnIfCapIdle(0x13, 0)`. It returns no
/// value.
s32 func_dryfield_garage_8017DA54(s32 arg0, s32 arg1, RoomEventMsg* msg)
{
    if ((msg->field_2 == 2) && (gGameSession->at4.loc.place != 1)) {
        Gp_SpawnIfCapIdle(0x13, 0);
    }
}

/// Task spawned from `D_dryfield_garage_8017DCAC`: spawns the second entry of
/// gameplay's `D_80141B6C`, keeping the task in `D_dryfield_garage_8018021C`,
/// then ends itself.
void func_dryfield_garage_8017DAA0(Task* arg0)
{
    switch (arg0->state) {
        case 0:
            D_dryfield_garage_8018021C = Task_SpawnFromTable(D_80141B6C, 1, 0, 0);
            arg0->state               += 1;
            break;
        case 1:
            taskKill(arg0);
            break;
    }
}

/// Entry state of the room task: installs the room's message table, registers
/// the task in pointer slot 7, sends message 0x3E9 with
/// `D_dryfield_garage_8017DCC4` to the task in pointer slot 0xA when arriving
/// by warp 2, moves nibble 0x155 from 1 to 2 (clearing nibble 3), clears bit 6
/// of `D_dryfield_garage_8017FD1C.field_4A` outside place 1, and advances to
/// the idle state.
void func_dryfield_garage_8017DB18(Task* arg0)
{
    arg0->msgTable = D_dryfield_garage_8017DC7C;
    Game_SetPtrSlot(arg0, 7);
    if ((gameGetPtrSlot(0xA) != NULL) && (gGameSession->at4.loc.warp == 2)) {
        Gp_DispatchMsg(gameGetPtrSlot(0xA), 0x3E9, (s32)&D_dryfield_garage_8017DCC4, 0);
    }
    if (GameFlag_GetNibble(0x155) == 1) {
        GameFlag_SetNibble(3, 0);
        GameFlag_SetNibble(0x155, 2);
    }
    if (gGameSession->at4.loc.place != 1) {
        D_dryfield_garage_8017FD1C.field_4A &= 0xBF;
    }
    arg0->state = arg0->state + 1;
}

/// Idle state of the room task.
void func_dryfield_garage_8017DC08(Task* task)
{
}

/// The room task: runs the state the task is in from a stack copy of the
/// room's three-state table.
void func_dryfield_garage_8017DC10(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_garage_8017D5DC;
    sp.funcs[task->state](task);
}

/// Empty function; nothing in the room references it.
void func_dryfield_garage_8017DC68(void)
{
}
