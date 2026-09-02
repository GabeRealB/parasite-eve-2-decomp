#include "common.h"

#include "gameplay/D4.h"

#include "main/session.h"
#include "main/task.h"

extern GpMsgEntry D_shelter_1f_bulwark_8018032C[];

INCLUDE_RODATA("rooms/nonmatchings/shelter_1f_bulwark/shelter_1f_bulwark", D_shelter_1f_bulwark_8017D5C0);

INCLUDE_ASM("rooms/nonmatchings/shelter_1f_bulwark/shelter_1f_bulwark", func_shelter_1f_bulwark_8017D61C);

INCLUDE_ASM("rooms/nonmatchings/shelter_1f_bulwark/shelter_1f_bulwark", func_shelter_1f_bulwark_8017D7B4);

INCLUDE_RODATA("rooms/nonmatchings/shelter_1f_bulwark/shelter_1f_bulwark", D_shelter_1f_bulwark_8017D5D8);

INCLUDE_ASM("rooms/nonmatchings/shelter_1f_bulwark/shelter_1f_bulwark", func_shelter_1f_bulwark_8017DA60);

s32 func_shelter_1f_bulwark_8017DBBC(void)
{
    return 0;
}

s32 func_shelter_1f_bulwark_8017DBC4(void)
{
    return 0;
}

s32 func_shelter_1f_bulwark_8017DBCC(void)
{
    return 0;
}

void func_shelter_1f_bulwark_8017DBD4(Task* arg0)
{
    arg0->field_24 = D_shelter_1f_bulwark_8018032C;
    Game_SetPtrSlot(arg0, 7);
    arg0->state = (s32)(arg0->state + 1);
}

void func_shelter_1f_bulwark_8017DC18(void)
{
}

INCLUDE_ASM("rooms/nonmatchings/shelter_1f_bulwark/shelter_1f_bulwark", func_shelter_1f_bulwark_8017DC20);
