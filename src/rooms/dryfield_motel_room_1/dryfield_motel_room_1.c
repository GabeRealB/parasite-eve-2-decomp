#include "common.h"

#include "main/gameflag.h"
#include "main/session.h"
#include "main/task.h"

#include "rooms/room_common.h"

extern TaskDesc D_dryfield_motel_room_1_8017E478;

INCLUDE_ASM("rooms/nonmatchings/dryfield_motel_room_1/dryfield_motel_room_1", func_dryfield_motel_room_1_8017D5EC);

INCLUDE_ASM("rooms/nonmatchings/dryfield_motel_room_1/dryfield_motel_room_1", func_dryfield_motel_room_1_8017D5F4);

INCLUDE_ASM("rooms/nonmatchings/dryfield_motel_room_1/dryfield_motel_room_1", func_dryfield_motel_room_1_8017D61C);

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

INCLUDE_ASM("rooms/nonmatchings/dryfield_motel_room_1/dryfield_motel_room_1", func_dryfield_motel_room_1_8017D69C);

void func_dryfield_motel_room_1_8017D74C(void)
{
}

INCLUDE_RODATA("rooms/nonmatchings/dryfield_motel_room_1/dryfield_motel_room_1", D_dryfield_motel_room_1_8017D5C0);

INCLUDE_RODATA("rooms/nonmatchings/dryfield_motel_room_1/dryfield_motel_room_1", RoomsShared8017d878Table);

INCLUDE_RODATA("rooms/nonmatchings/dryfield_motel_room_1/dryfield_motel_room_1", jtbl_dryfield_motel_room_1_8017D5D0);
