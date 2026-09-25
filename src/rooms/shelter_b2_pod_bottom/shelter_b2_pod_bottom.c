#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/session.h"
#include "main/task.h"
#include "rooms/room.h"
#include "rooms/room_common.h"

void       func_80162B0C(s32);
extern s32 func_80179A04(RoomEventMsg* in, RoomEventMsg* out);

extern u8         D_80165F48;
extern u8         D_80166848;
extern GpMsgEntry D_shelter_b2_pod_bottom_80181C6C[];

void func_shelter_b2_pod_bottom_8017D648(Task* arg0);
void func_shelter_b2_pod_bottom_8017D6F8(Task* task);

/// The room task's states: set up, idle, then `taskKill`.
const TaskFuncTable3 D_shelter_b2_pod_bottom_8017D5C4 = {
    { func_shelter_b2_pod_bottom_8017D648, func_shelter_b2_pod_bottom_8017D6F8, taskKill },
};

/// The room's handler for message 0x13F1: does nothing and returns 0.
s32 func_shelter_b2_pod_bottom_8017D5EC(void)
{
    return 0;
}

/// The room's handler for message 0x13EE: copies the incoming record onto the
/// outgoing one, passes both to `func_80179A04` and returns 1.
s32 func_shelter_b2_pod_bottom_8017D5F4(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    *out = *in;
    func_80179A04(in, out);
    return 1;
}

/// The room's handler for message 0x13F0: does nothing and returns 0.
s32 func_shelter_b2_pod_bottom_8017D638(void)
{
    return 0;
}

/// The room's handler for message 0x13EF: does nothing and returns 0.
s32 func_shelter_b2_pod_bottom_8017D640(void)
{
    return 0;
}

/// The room task's setup state: installs the room's message table, stores the
/// task in pointer slot 7 and, on place 1, calls `func_80162B0C` and
/// `func_800E8634`; elsewhere it sends message 0x7DB to the slot-4 task.
void func_shelter_b2_pod_bottom_8017D648(Task* arg0)
{
    RoomActorMsg msg;

    arg0->msgTable = D_shelter_b2_pod_bottom_80181C6C;
    Game_SetPtrSlot(arg0, 7);
    if (gGameSession->at4.loc.place == 1) {
        func_80162B0C(0);
        func_800E8634((s32)&D_80165F48, 0, (s32)&D_80166848);
    } else {
        msg.from.loc.stage = 0;
        msg.from.loc.area  = 0;
        msg.command        = 7;
        Gp_DispatchMsg((Task*)Gp_LookupSlot4(0), 0x7DB, (s32)&msg, 0);
    }
    arg0->state++;
}

/// The room task's idle state: does nothing. The unused local reproduces the
/// original's stack frame.
void func_shelter_b2_pod_bottom_8017D6F8(Task* task)
{
    char pad[0x10];
}

/// The room task: copies its three-state table to the stack and runs the
/// entry the task's state selects.
void func_shelter_b2_pod_bottom_8017D708(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_shelter_b2_pod_bottom_8017D5C4;
    sp.funcs[task->state](task);
}
