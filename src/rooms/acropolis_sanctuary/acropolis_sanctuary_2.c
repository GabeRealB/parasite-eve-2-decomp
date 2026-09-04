#include "common.h"

#include "gameplay/3CD8.h"

#include "main/gameflag.h"
#include "main/task.h"

#include "rooms/room_common.h"

extern u8       D_acropolis_sanctuary_80181814[];
extern TaskDesc D_acropolis_sanctuary_80182240;

extern void func_acropolis_sanctuary_8017DF88(s32 arg0, s32 arg1);

s32 func_acropolis_sanctuary_8017D810(s32 arg0, s32 arg1, s32 arg2)
{
    if (arg2 == 0 && GameFlag_GetNibble(6) == 0) {
        func_800E8614((s32)&D_acropolis_sanctuary_80181814, 0);
    }
    return 0;
}

/// Message gate for the sanctuary hotspot registered under id 0x13EF: sub-id 1
/// arms the room's own task the first time it is seen, latching nibble 7 so a
/// second visit does nothing. The record is not copied to the outgoing one -
/// this handler only ever consumes the message (returns 0).
s32 func_acropolis_sanctuary_8017D848(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    if (in->field_2 == 1 && GameFlag_GetNibble(7) == 0) {
        GameFlag_SetNibble(7, 1);
        Task_SpawnFromTable(&D_acropolis_sanctuary_80182240, 0, 0, 0);
    }
    return 0;
}

void func_acropolis_sanctuary_8017D8A0(u32 arg0)
{
    func_acropolis_sanctuary_8017DF88((arg0 >> 8) & 0xFF, arg0 & 0xFF);
}

INCLUDE_ASM("rooms/nonmatchings/acropolis_sanctuary/acropolis_sanctuary_2", func_acropolis_sanctuary_8017D8CC);

INCLUDE_ASM("rooms/nonmatchings/acropolis_sanctuary/acropolis_sanctuary_2", func_acropolis_sanctuary_8017D930);

INCLUDE_ASM("rooms/nonmatchings/acropolis_sanctuary/acropolis_sanctuary_2", func_acropolis_sanctuary_8017D9E8);

INCLUDE_ASM("rooms/nonmatchings/acropolis_sanctuary/acropolis_sanctuary_2", func_acropolis_sanctuary_8017DA40);

INCLUDE_ASM("rooms/nonmatchings/acropolis_sanctuary/acropolis_sanctuary_2", func_acropolis_sanctuary_8017DCE0);

INCLUDE_ASM("rooms/nonmatchings/acropolis_sanctuary/acropolis_sanctuary_2", func_acropolis_sanctuary_8017DD78);

INCLUDE_ASM("rooms/nonmatchings/acropolis_sanctuary/acropolis_sanctuary_2", func_acropolis_sanctuary_8017DF88);

INCLUDE_ASM("rooms/nonmatchings/acropolis_sanctuary/acropolis_sanctuary_2", func_acropolis_sanctuary_8017E00C);

INCLUDE_ASM("rooms/nonmatchings/acropolis_sanctuary/acropolis_sanctuary_2", func_acropolis_sanctuary_8017E134);

INCLUDE_ASM("rooms/nonmatchings/acropolis_sanctuary/acropolis_sanctuary_2", func_acropolis_sanctuary_8017E338);

INCLUDE_ASM("rooms/nonmatchings/acropolis_sanctuary/acropolis_sanctuary_2", func_acropolis_sanctuary_8017EC90);

INCLUDE_ASM("rooms/nonmatchings/acropolis_sanctuary/acropolis_sanctuary_2", func_acropolis_sanctuary_8017F4E8);

INCLUDE_ASM("rooms/nonmatchings/acropolis_sanctuary/acropolis_sanctuary_2", func_acropolis_sanctuary_8017F918);

INCLUDE_ASM("rooms/nonmatchings/acropolis_sanctuary/acropolis_sanctuary_2", func_acropolis_sanctuary_8017F974);

INCLUDE_ASM("rooms/nonmatchings/acropolis_sanctuary/acropolis_sanctuary_2", func_acropolis_sanctuary_8017FB18);

INCLUDE_ASM("rooms/nonmatchings/acropolis_sanctuary/acropolis_sanctuary_2", func_acropolis_sanctuary_80180264);

INCLUDE_ASM("rooms/nonmatchings/acropolis_sanctuary/acropolis_sanctuary_2", func_acropolis_sanctuary_801802E0);
