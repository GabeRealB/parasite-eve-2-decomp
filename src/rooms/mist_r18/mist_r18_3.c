#include "common.h"
#include "main/fs.h"
#include "main/session.h"
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

void func_mist_r18_8017EBB8(void)
{
    Game_Session->field_52 = 1;
    CdCmd_StartOverlay(1U, 0x1EU, 0xBU);
    CdCmd_EnqueueReplaceOverlay82();
}

INCLUDE_ASM("rooms/nonmatchings/mist_r18/mist_r18_3", func_mist_r18_8017EBF8);
