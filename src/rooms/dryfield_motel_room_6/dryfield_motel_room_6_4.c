#include "common.h"

#include "gameplay/1A8.h"
#include "main/gameflag.h"
#include "main/session.h"
#include "main/task.h"

extern TaskDesc D_dryfield_motel_room_6_80182D78;

void func_dryfield_motel_room_6_80181910(void)
{
}

s32 func_dryfield_motel_room_6_80181918(void)
{
    return 0;
}

INCLUDE_ASM("rooms/nonmatchings/dryfield_motel_room_6/dryfield_motel_room_6_4", func_dryfield_motel_room_6_80181920);

/// Handler for a slot-7 msg `0x13EF` request (`GpMsg13EF`) whose sub-id
/// (`field_2`) is clear: the first time it runs it latches nibble 0x31 and
/// arms the room's script task from `D_dryfield_motel_room_6_80182D78`.
/// Where the sibling gates of this shape (`func_acropolis_security_room_8017D740`,
/// `func_acropolis_sanctuary_8017D848`) answer 0, this one answers 1.
s32 func_dryfield_motel_room_6_801819A8(Task* arg0, s32 arg1, GpMsg13EF* arg2)
{
    if (arg2->field_2 == 0 && GameFlag_GetNibble(0x31) == 0) {
        GameFlag_SetNibble(0x31, 1);
        Task_SpawnFromTable(&D_dryfield_motel_room_6_80182D78, 0, 0, 0);
    }
    return 1;
}

s32 func_dryfield_motel_room_6_80181A00(void)
{
    return 0;
}

INCLUDE_ASM("rooms/nonmatchings/dryfield_motel_room_6/dryfield_motel_room_6_4", func_dryfield_motel_room_6_80181A08);
