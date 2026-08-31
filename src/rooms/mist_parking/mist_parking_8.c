#include "common.h"
#include "main/task.h"
extern Task* D_mist_parking_80195320;

void func_mist_parking_801830F8(void)
{
}

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking_8", func_mist_parking_80183100);

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking_8", func_mist_parking_8018312C);

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking_8", func_mist_parking_8018316C);

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking_8", func_mist_parking_801831F0);

void func_mist_parking_8018326C(s32 arg0)
{
    if (arg0 == 0) {
        if (D_mist_parking_80195320 != NULL) {
            Task_Kill(D_mist_parking_80195320);
        }
        D_mist_parking_80195320 = NULL;
    }
}

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking_8", func_mist_parking_801832AC);

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking_8", func_mist_parking_80183304);
