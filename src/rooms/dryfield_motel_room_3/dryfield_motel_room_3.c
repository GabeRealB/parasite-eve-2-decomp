#include "common.h"

#include "gameplay/D4.h"

#include "main/session.h"
#include "main/task.h"

extern GpMsgEntry D_dryfield_motel_room_3_8017D6B4[];

void func_dryfield_motel_room_3_8017D610(Task* arg0)
{
    arg0->field_24 = D_dryfield_motel_room_3_8017D6B4;
    Game_SetPtrSlot(arg0, 7);
    arg0->state = (s32)(arg0->state + 1);
}

void func_dryfield_motel_room_3_8017D654(void)
{
}

INCLUDE_RODATA("rooms/nonmatchings/dryfield_motel_room_3/dryfield_motel_room_3", D_dryfield_motel_room_3_8017D5C0);

INCLUDE_ASM("rooms/nonmatchings/dryfield_motel_room_3/dryfield_motel_room_3", func_dryfield_motel_room_3_8017D65C);
