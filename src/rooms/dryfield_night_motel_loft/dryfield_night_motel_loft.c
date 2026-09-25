#include "common.h"

#include "gameplay/268.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"

#include "main/gameflag.h"
#include "main/session.h"
#include "main/task.h"

#include "rooms/dryfield_night_motel_loft.h"
#include "rooms/room.h"
#include "rooms/room_common.h"

extern u8 D_8007216D;
extern s8 D_8007272D;
extern u8 D_80115680;

/// The room's message table, published at `Task::msgTable` by the room task.
extern GpMsgEntry D_dryfield_night_motel_loft_8017EB1C[];

/// One-entry task table spawning the loft's cap-script task
/// (`func_dryfield_night_motel_loft_8017D6F8`).
extern TaskDesc D_dryfield_night_motel_loft_8017EB4C[];

extern u8      D_dryfield_night_motel_loft_8017EB78[];
extern GpObj4A D_dryfield_night_motel_loft_80180440;

/// The room's 0x7DB payload buffer.
extern RoomActorMsg D_dryfield_night_motel_loft_8018092C;

void func_dryfield_night_motel_loft_8017D6F8(Task* arg0);

/// Message-table handler for id 0x13F1: accepts the message and does nothing.
s32 func_dryfield_night_motel_loft_8017D5F8(void)
{
    return 0;
}

/// Message-table handler for id 0x13EE: echoes the incoming record into the
/// reply and, for a message 0x1D that is not report-only (`field_5 == 0`),
/// answers 1 while game nibble 0x61 is clear and 3 once it is set. Returns 1.
s32 func_dryfield_night_motel_loft_8017D600(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    s32 nib;

    *out = *in;
    if (in->msgId == 0x1D && in->field_5 == 0) {
        nib = GameFlag_GetNibble(0x61);
        if (nib == 0) {
            nib = 1;
        } else {
            nib = 3;
        }
        out->field_3 = nib;
    }
    return 1;
}

/// Message-table handler for id 0x13F0: on command 3 (`arg2`) silences the
/// player's weapon and spawns the loft's cap-script task. Other commands do
/// nothing.
s32 func_dryfield_night_motel_loft_8017D67C(Task* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    if (arg2 == 3) {
        Gp_MsgPlayerWeapon(0);
        Task_SpawnFromTable(D_dryfield_night_motel_loft_8017EB4C, 0, 0, 0);
    }
    return 0;
}

/// Message-table handler for id 0x13EF: accepts the message and does nothing.
s32 func_dryfield_night_motel_loft_8017D6BC(void)
{
    return 0;
}

/// Message-table handler for id 0x13F2: queues stage sound 0x531F0005 on
/// command 5.
s32 func_dryfield_night_motel_loft_8017D6C4(Task* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    if (arg2 == 5) {
        Gp_EnqueueStageSnd6(0x531F0000 | 5, 0, 0);
    }
    return 0;
}

/// The loft's cap-script task: runs cap command 0x12 once game nibble 0x170 is
/// set and 3 before that, waits for it to finish, sets the nibble when the
/// script ended on event key 0x1F, then gives the player's weapon back and
/// kills itself.
void func_dryfield_night_motel_loft_8017D6F8(Task* arg0)
{
    switch (arg0->state) {
        case 0:
            Gp_StateF0.field_4 = 1;
            Gp_RunCapCmd(GameFlag_GetNibble(0x170) != 0 ? 0x12 : 3, 0);
            D_80115680  = 5;
            arg0->state = arg0->state + 1;
            return;
        case 1:
            if (Gp_CapBusy() == 0) {
                arg0->state = arg0->state + 1;
                return;
            }
            return;
        case 2:
            if (Gp_GetCapEventKey() == 0x1F) {
                GameFlag_SetNibble(0x170, 1);
            }
            Gp_StateF0.field_4 = 0;
            Gp_MsgPlayerWeapon(1);
            taskKill(arg0);
            break;
    }
}

/// Sets the session's current room to `arg0` and mirrors it in `D_8007216D`.
void func_dryfield_night_motel_loft_8017D7EC(u8 arg0)
{
    gGameSession->at4.loc.room = arg0;
    D_8007216D                 = arg0;
}

/// First state of the room task: publishes the room's message table, claims
/// pointer slot 7 and, once the slot-4 task exists and game nibble 0x96 is set,
/// raises the 0x7DB payload's halfword and sends it to that task. It then sets
/// the grid for flag 0xA and advances to the next state.
void func_dryfield_night_motel_loft_8017D808(Task* arg0)
{
    arg0->msgTable = D_dryfield_night_motel_loft_8017EB1C;
    Game_SetPtrSlot(arg0, 7);
    if (Gp_LookupSlot4(0) != 0 && GameFlag_GetNibble(0x96) != 0) {
        D_dryfield_night_motel_loft_8018092C.command = 1;
        Gp_DispatchMsg(Gp_LookupSlot4(0), 0x7DB, (s32)&D_dryfield_night_motel_loft_8018092C, 0);
    }
    func_dryfield_night_motel_loft_8017D9BC(Gp_GetCurBit2Flag(0xA) == 2);
    arg0->state = (s32)(arg0->state + 1);
}

/// Second state of the room task, run every frame: keeps the grid in step with
/// flag 0xA, clears bit 0x40 of `field_4A` while the flag is 2, and the first
/// time collected bit 0x117 is seen with nibble 0x96 still clear and the slot-4
/// task present, sets the nibble and starts the room's event.
void func_dryfield_night_motel_loft_8017D8B0(Task* arg0)
{
    func_dryfield_night_motel_loft_8017D9BC(Gp_GetCurBit2Flag(0xA) == 2);
    if (Gp_GetCurBit2Flag(0xA) == 2) {
        D_dryfield_night_motel_loft_80180440.field_4A &= 0xBF;
    }
    if (Gp_HasCollectedBit(0x117) && GameFlag_GetNibble(0x96) == 0 && Gp_LookupSlot4(0)) {
        GameFlag_SetNibble(0x96, 1);
        func_800E8614((s32)&D_dryfield_night_motel_loft_8017EB78, 0);
        func_800E3FAC(0xA2, 0x15);
        D_8007272D = 3;
    }
}

/// The room task's three states.
const TaskFuncTable3 D_dryfield_night_motel_loft_8017D5C4 = {
    { func_dryfield_night_motel_loft_8017D808, func_dryfield_night_motel_loft_8017D8B0, taskKill },
};

/// The room task's callback: runs the state `Task::state` selects from a
/// stack copy of `D_dryfield_night_motel_loft_8017D5C4`.
void func_dryfield_night_motel_loft_8017D964(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_night_motel_loft_8017D5C4;
    sp.funcs[task->state](task);
}
