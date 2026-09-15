#include "common.h"

#include "gameplay/D4.h"

#include "main/gameflag.h"
#include "main/session.h"
#include "main/task.h"

#include "rooms/dryfield_motel_room_1.h"
#include "rooms/room_common.h"

extern GpMsgEntry D_dryfield_motel_room_1_8017E0A8[];
extern TaskDesc   D_dryfield_motel_room_1_8017E478;

s32 func_dryfield_motel_room_1_8017D5EC(void)
{
    return 0;
}
INCLUDE_ASM("rooms/nonmatchings/dryfield_motel_room_1/dryfield_motel_room_1", func_dryfield_motel_room_1_8017D5F4);

s32 func_dryfield_motel_room_1_8017D61C(void)
{
    return 0;
}
/// Message gate for the room's hotspot registered under id 0x13EF - the id the
/// sanctuary's own gate uses. On the phase-3 visit (`Game_Session::field_9`)
/// whose sub-id (`RoomEventMsg::field_2`) is 1 and that has not yet latched
/// nibble 0x5C, it arms the room's script task and latches the nibble. The
/// record is not copied to the outgoing one: this handler only ever consumes
/// the message (returns 0).
///
/// GCC hoists the `Game_Session` load above the `addiu $sp` prologue, which is
/// why the function starts two instructions before its frame setup.
s32 func_dryfield_motel_room_1_8017D624(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    if (Game_Session->field_9 == 3 && GameFlag_GetNibble(0x5C) == 0 && in->field_2 == 1) {
        GameFlag_SetNibble(0x5C, 1);
        Task_SpawnFromTable(&D_dryfield_motel_room_1_8017E478, 0, 0, 0);
    }
    return 0;
}

/// Room entry task tick: park the room's hotspot table in `Task::field_24` -
/// the table whose 0x13EF entry is the gate `func_dryfield_motel_room_1_8017D624`
/// matches - register the task in pointer slot 7, then, on the phase-3 visit
/// whose nibble 0x5C is still clear, announce the room to the slot-4 task with
/// message 0x7DA carrying the session's two id bytes and a zero halfword. Then
/// advance state.
void func_dryfield_motel_room_1_8017D69C(Task* arg0)
{
    Dmr1Msg7DA msg;

    arg0->field_24 = D_dryfield_motel_room_1_8017E0A8;
    Game_SetPtrSlot(arg0, 7);
    if (Game_Session->field_9 == 3 && GameFlag_GetNibble(0x5C) == 0) {
        msg.field_0 = Game_Session->field_7;
        msg.field_1 = Game_Session->field_6;
        msg.field_2 = 0;
        Gp_DispatchMsg(Game_GetPtrSlot(4), 0x7DA, (s32)&msg, 0x7DB);
    }
    arg0->state = arg0->state + 1;
}
void func_dryfield_motel_room_1_8017D74C(void)
{
}

INCLUDE_RODATA("rooms/nonmatchings/dryfield_motel_room_1/dryfield_motel_room_1", D_dryfield_motel_room_1_8017D5C0);

INCLUDE_RODATA("rooms/nonmatchings/dryfield_motel_room_1/dryfield_motel_room_1", RoomsShared8017d878Table);

INCLUDE_RODATA("rooms/nonmatchings/dryfield_motel_room_1/dryfield_motel_room_1", jtbl_dryfield_motel_room_1_8017D5D0);
