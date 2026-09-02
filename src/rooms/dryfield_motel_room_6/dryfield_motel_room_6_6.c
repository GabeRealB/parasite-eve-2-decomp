#include "common.h"

#include "main/session.h"
#include "main/task.h"

extern u8  D_80115598;
extern s32 D_dryfield_motel_room_6_80182D48;

INCLUDE_ASM("rooms/nonmatchings/dryfield_motel_room_6/dryfield_motel_room_6_6", func_dryfield_motel_room_6_80181740);

INCLUDE_ASM("rooms/nonmatchings/dryfield_motel_room_6/dryfield_motel_room_6_6", func_dryfield_motel_room_6_80181880);

void func_dryfield_motel_room_6_80181910(void)
{
}

s32 func_dryfield_motel_room_6_80181918(void)
{
    return 0;
}

INCLUDE_ASM("rooms/nonmatchings/dryfield_motel_room_6/dryfield_motel_room_6_6", func_dryfield_motel_room_6_80181920);

INCLUDE_ASM("rooms/nonmatchings/dryfield_motel_room_6/dryfield_motel_room_6_6", func_dryfield_motel_room_6_801819A8);

s32 func_dryfield_motel_room_6_80181A00(void)
{
    return 0;
}

INCLUDE_ASM("rooms/nonmatchings/dryfield_motel_room_6/dryfield_motel_room_6_6", func_dryfield_motel_room_6_80181A08);

void func_dryfield_motel_room_6_80181AC4(Task* arg0)
{
    arg0->field_24 = &D_dryfield_motel_room_6_80182D48;
    Game_SetPtrSlot(arg0, 7);
    arg0->state = (s32)(arg0->state + 1);
    D_80115598  = 1;
}

void func_dryfield_motel_room_6_80181B10(void)
{
}

INCLUDE_ASM("rooms/nonmatchings/dryfield_motel_room_6/dryfield_motel_room_6_6", func_dryfield_motel_room_6_80181B18);
