#include "common.h"

#include "gameplay/D4.h"

#include "main/session.h"
#include "main/task.h"

extern GpMsgEntry D_dryfield_warehouse_8017F554[];
extern TaskDesc   D_dryfield_warehouse_8017F56C[];

INCLUDE_ASM("rooms/nonmatchings/dryfield_warehouse/dryfield_warehouse", func_dryfield_warehouse_8017D5E8);

INCLUDE_ASM("rooms/nonmatchings/dryfield_warehouse/dryfield_warehouse", func_dryfield_warehouse_8017D764);

INCLUDE_ASM("rooms/nonmatchings/dryfield_warehouse/dryfield_warehouse", func_dryfield_warehouse_8017D824);

INCLUDE_ASM("rooms/nonmatchings/dryfield_warehouse/dryfield_warehouse", func_dryfield_warehouse_8017D8D4);

void func_dryfield_warehouse_8017D99C(Task* arg0)
{
    arg0->field_24 = D_dryfield_warehouse_8017F554;
    Game_SetPtrSlot(arg0, 7);
    Task_SpawnFromTable(D_dryfield_warehouse_8017F56C, 1, 0, 0);
    arg0->state = (s32)(arg0->state + 1);
}

void func_dryfield_warehouse_8017D9F8(void)
{
}

INCLUDE_RODATA("rooms/nonmatchings/dryfield_warehouse/dryfield_warehouse", D_dryfield_warehouse_8017D5C0);

INCLUDE_RODATA("rooms/nonmatchings/dryfield_warehouse/dryfield_warehouse", RoomsShared8017d878Table);

INCLUDE_RODATA("rooms/nonmatchings/dryfield_warehouse/dryfield_warehouse", jtbl_dryfield_warehouse_8017D5D0);
