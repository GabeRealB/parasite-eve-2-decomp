#include "common.h"
#include "main/display.h"
extern Task*    D_dryfield_dilapidated_house_801857E8;
extern TaskDesc D_dryfield_dilapidated_house_80186854;

INCLUDE_ASM("rooms/nonmatchings/dryfield_dilapidated_house/dryfield_dilapidated_house_2", func_dryfield_dilapidated_house_8017E8E8);

INCLUDE_ASM("rooms/nonmatchings/dryfield_dilapidated_house/dryfield_dilapidated_house_2", func_dryfield_dilapidated_house_8017E970);

INCLUDE_ASM("rooms/nonmatchings/dryfield_dilapidated_house/dryfield_dilapidated_house_2", func_dryfield_dilapidated_house_8017E9A4);

void func_dryfield_dilapidated_house_8017EA10(s32 arg0)
{
    if (arg0 != 0) {
        D_dryfield_dilapidated_house_801857E8 = Task_SpawnFromTable(&D_dryfield_dilapidated_house_80186854, 0, 3, Game_GetPtrSlot(3));
        return;
    }
    if (D_dryfield_dilapidated_house_801857E8 != NULL) {
        Task_Kill(D_dryfield_dilapidated_house_801857E8);
        D_dryfield_dilapidated_house_801857E8 = NULL;
    }
}
