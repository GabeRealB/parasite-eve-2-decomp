#include "common.h"
#include "main/task.h"
extern Task* D_mist_r18_80186E98;

INCLUDE_ASM("rooms/nonmatchings/mist_r18/mist_r18_3", func_mist_r18_8017EA2C);

void func_mist_r18_8017EA60(void)
{
    if (D_mist_r18_80186E98 != NULL) {
        Task_Kill(D_mist_r18_80186E98);
    }
    D_mist_r18_80186E98 = NULL;
}

INCLUDE_ASM("rooms/nonmatchings/mist_r18/mist_r18_3", func_mist_r18_8017EA98);

INCLUDE_ASM("rooms/nonmatchings/mist_r18/mist_r18_3", func_mist_r18_8017EB48);

INCLUDE_ASM("rooms/nonmatchings/mist_r18/mist_r18_3", func_mist_r18_8017EBB8);

INCLUDE_ASM("rooms/nonmatchings/mist_r18/mist_r18_3", func_mist_r18_8017EBF8);
