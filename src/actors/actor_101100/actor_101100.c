#include "common.h"
#include "main/task.h"

INCLUDE_ASM("actors/nonmatchings/actor_101100/actor_101100", func_actor_101100_80131F08);

INCLUDE_ASM("actors/nonmatchings/actor_101100/actor_101100", func_actor_101100_80132250);

INCLUDE_RODATA("actors/nonmatchings/actor_101100/actor_101100", D_actor_101100_80131E20);

INCLUDE_RODATA("actors/nonmatchings/actor_101100/actor_101100", D_actor_101100_80131E24);

INCLUDE_ASM("actors/nonmatchings/actor_101100/actor_101100", func_actor_101100_8013279C);

INCLUDE_ASM("actors/nonmatchings/actor_101100/actor_101100", func_actor_101100_80132B10);

INCLUDE_ASM("actors/nonmatchings/actor_101100/actor_101100", func_actor_101100_80132D78);

INCLUDE_ASM("actors/nonmatchings/actor_101100/actor_101100", func_actor_101100_801339B0);

INCLUDE_ASM("actors/nonmatchings/actor_101100/actor_101100", func_actor_101100_80133BB8);

INCLUDE_ASM("actors/nonmatchings/actor_101100/actor_101100", func_actor_101100_80134780);

INCLUDE_ASM("actors/nonmatchings/actor_101100/actor_101100", func_actor_101100_80135404);

INCLUDE_ASM("actors/nonmatchings/actor_101100/actor_101100", func_actor_101100_80135560);

INCLUDE_ASM("actors/nonmatchings/actor_101100/actor_101100", func_actor_101100_801356BC);

INCLUDE_ASM("actors/nonmatchings/actor_101100/actor_101100", func_actor_101100_801357F0);

INCLUDE_ASM("actors/nonmatchings/actor_101100/actor_101100", func_actor_101100_801359CC);

INCLUDE_ASM("actors/nonmatchings/actor_101100/actor_101100", func_actor_101100_80135FDC);

INCLUDE_ASM("actors/nonmatchings/actor_101100/actor_101100", func_actor_101100_80136230);

INCLUDE_ASM("actors/nonmatchings/actor_101100/actor_101100", func_actor_101100_801366E8);

INCLUDE_ASM("actors/nonmatchings/actor_101100/actor_101100", func_actor_101100_80136BD4);

INCLUDE_ASM("actors/nonmatchings/actor_101100/actor_101100", func_actor_101100_80136F8C);

INCLUDE_ASM("actors/nonmatchings/actor_101100/actor_101100", func_actor_101100_80137498);

INCLUDE_ASM("actors/nonmatchings/actor_101100/actor_101100", func_actor_101100_80137B1C);

INCLUDE_ASM("actors/nonmatchings/actor_101100/actor_101100", func_actor_101100_80137C88);

INCLUDE_ASM("actors/nonmatchings/actor_101100/actor_101100", func_actor_101100_80137FB8);

INCLUDE_ASM("actors/nonmatchings/actor_101100/actor_101100", func_actor_101100_801381AC);

INCLUDE_ASM("actors/nonmatchings/actor_101100/actor_101100", func_actor_101100_80138374);

INCLUDE_ASM("actors/nonmatchings/actor_101100/actor_101100", func_actor_101100_80138404);

INCLUDE_ASM("actors/nonmatchings/actor_101100/actor_101100", func_actor_101100_8013845C);

INCLUDE_ASM("actors/nonmatchings/actor_101100/actor_101100", func_actor_101100_801384AC);

INCLUDE_ASM("actors/nonmatchings/actor_101100/actor_101100", func_actor_101100_8013852C);

INCLUDE_ASM("actors/nonmatchings/actor_101100/actor_101100", func_actor_101100_801385E0);

INCLUDE_ASM("actors/nonmatchings/actor_101100/actor_101100", func_actor_101100_80138774);

INCLUDE_ASM("actors/nonmatchings/actor_101100/actor_101100", func_actor_101100_801388E8);

INCLUDE_ASM("actors/nonmatchings/actor_101100/actor_101100", func_actor_101100_8013898C);

INCLUDE_ASM("actors/nonmatchings/actor_101100/actor_101100", func_actor_101100_80138A2C);

INCLUDE_ASM("actors/nonmatchings/actor_101100/actor_101100", func_actor_101100_80138B5C);

INCLUDE_ASM("actors/nonmatchings/actor_101100/actor_101100", func_actor_101100_80138C6C);

INCLUDE_ASM("actors/nonmatchings/actor_101100/actor_101100", func_actor_101100_80138D58);

INCLUDE_ASM("actors/nonmatchings/actor_101100/actor_101100", func_actor_101100_80138E34);

INCLUDE_ASM("actors/nonmatchings/actor_101100/actor_101100", func_actor_101100_80138EFC);

INCLUDE_ASM("actors/nonmatchings/actor_101100/actor_101100", func_actor_101100_80138F68);

INCLUDE_ASM("actors/nonmatchings/actor_101100/actor_101100", func_actor_101100_801390D8);

void func_actor_101100_8013918C(Task* arg0)
{
    u16 temp_v0;

    temp_v0             = arg0->killCountdown - 1;
    arg0->killCountdown = temp_v0;
    if ((temp_v0 << 0x10) <= 0) {
        Task_CallExit(arg0);
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_101100/actor_101100", func_actor_101100_801391C8);

INCLUDE_ASM("actors/nonmatchings/actor_101100/actor_101100", func_actor_101100_801391FC);
