#include "common.h"

#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/fs.h"
#include "main/session.h"
#include "main/task.h"
#include "rooms/room_common.h"

extern void func_80179B14(RoomEventMsg* in, RoomEventMsg* out);

/// The room's message table, which state 0 of its event task installs.
extern GpMsgEntry D_neo_ark_eve_elevator_8017D724[];

void func_neo_ark_eve_elevator_8017D678(Task* task);
void func_neo_ark_eve_elevator_8017D6BC(Task* task);

/// The event task's three states: install the message table, idle, and kill.
const TaskFuncTable3 D_neo_ark_eve_elevator_8017D5C4 = {
    {
        func_neo_ark_eve_elevator_8017D678,
        func_neo_ark_eve_elevator_8017D6BC,
        taskKill,
    },
};

/// The room's handler for message 0x13F1: does nothing and returns 0.
s32 func_neo_ark_eve_elevator_8017D5D0(void)
{
    return 0;
}

/// The room's handler for message 0x13EE: copies the incoming record onto the
/// outgoing one and passes both to `func_80179B14`. It returns 1 unless the
/// record's `msgId` is 0x18 and `CdCmd_IsIdle` returns 0; in that case it
/// returns 0, first starting cap event 1 through `Gp_SpawnIfCapIdle` when the
/// record's `field_5` is 0.
s32 func_neo_ark_eve_elevator_8017D5D8(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    *out = *in;
    func_80179B14(in, out);
    if (in->msgId != 0x18) {
        return 1;
    }
    if (CdCmd_IsIdle() != 0) {
        return 1;
    }
    if (in->field_5 != 0) {
        return 0;
    }
    Gp_SpawnIfCapIdle(1, 1);
    return 0;
}

/// The room's handler for message 0x13F0: does nothing and returns 0.
s32 func_neo_ark_eve_elevator_8017D668(void)
{
    return 0;
}

/// The room's handler for message 0x13EF: does nothing and returns 0.
s32 func_neo_ark_eve_elevator_8017D670(void)
{
    return 0;
}

/// State 0 of the room's event task: installs the room's message table,
/// publishes the task in pointer slot 7 and advances to state 1.
void func_neo_ark_eve_elevator_8017D678(Task* task)
{
    task->msgTable = D_neo_ark_eve_elevator_8017D724;
    Game_SetPtrSlot(task, 7);
    task->state = (s32)(task->state + 1);
}

/// State 1 of the room's event task: does nothing, so the task idles here.
void func_neo_ark_eve_elevator_8017D6BC(Task* task)
{
}

/// The room's event task: runs the handler for its current state, through a
/// stack copy of the state table.
void func_neo_ark_eve_elevator_8017D6C4(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_neo_ark_eve_elevator_8017D5C4;
    sp.funcs[task->state](task);
}

/// An empty function nothing in the room's tables names.
void func_neo_ark_eve_elevator_8017D71C(void)
{
}
