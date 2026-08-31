#include "common.h"
#include "main/task.h"

INCLUDE_ASM("aya/nonmatchings/aya_20900/aya_20900", func_aya_20900_8011578C);

INCLUDE_ASM("aya/nonmatchings/aya_20900/aya_20900", func_aya_20900_80115948);

INCLUDE_RODATA("aya/nonmatchings/aya_20900/aya_20900", D_aya_20900_80115770);

INCLUDE_ASM("aya/nonmatchings/aya_20900/aya_20900", func_aya_20900_80115A14);

s32 func_aya_20900_80115A14(Task*); /* extern */

void func_aya_20900_80115CFC(Task* arg0)
{
    TaskIdMap* temp_v0;
    s32        temp_v1;

    temp_v1 = arg0->state;
    switch (temp_v1) { /* irregular */
        case 0:
            temp_v0     = Mem_Malloc(8U, false);
            arg0->idMap = temp_v0;
            if (temp_v0 == NULL) {
                Task_Kill(arg0);
                return;
            }
            Mem_Set(temp_v0, 0U, 8U);
            arg0->state += 1;
        case 1:
            if ((func_aya_20900_80115A14(arg0) << 0x10) != 0) {
                Task_RequestKill(arg0, 0);
            }
            return;
    }
}
