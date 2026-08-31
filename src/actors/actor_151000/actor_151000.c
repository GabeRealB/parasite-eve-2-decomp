#include "common.h"
#include "main/task.h"
extern TaskDesc D_actor_151000_80133360;
extern s32      D_actor_151000_8013D378;

INCLUDE_ASM("actors/nonmatchings/actor_151000/actor_151000", func_actor_151000_80131E2C);

void func_actor_151000_80131EE0(s32 arg0)
{
    D_actor_151000_8013D378 = arg0;
    if (arg0 != 0) {
        Task_SpawnFromTable(&D_actor_151000_80133360, 0, 0, 0);
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_151000/actor_151000", func_actor_151000_80131F1C);

INCLUDE_ASM("actors/nonmatchings/actor_151000/actor_151000", func_actor_151000_80132084);

INCLUDE_ASM("actors/nonmatchings/actor_151000/actor_151000", func_actor_151000_801323F4);

INCLUDE_ASM("actors/nonmatchings/actor_151000/actor_151000", func_actor_151000_80132450);

INCLUDE_ASM("actors/nonmatchings/actor_151000/actor_151000", func_actor_151000_801324D4);

INCLUDE_ASM("actors/nonmatchings/actor_151000/actor_151000", func_actor_151000_801324FC);

INCLUDE_ASM("actors/nonmatchings/actor_151000/actor_151000", func_actor_151000_801325C4);

INCLUDE_ASM("actors/nonmatchings/actor_151000/actor_151000", func_actor_151000_80132610);

INCLUDE_ASM("actors/nonmatchings/actor_151000/actor_151000", func_actor_151000_801326AC);

INCLUDE_ASM("actors/nonmatchings/actor_151000/actor_151000", func_actor_151000_80132738);

INCLUDE_ASM("actors/nonmatchings/actor_151000/actor_151000", func_actor_151000_801327C8);

INCLUDE_ASM("actors/nonmatchings/actor_151000/actor_151000", func_actor_151000_80132810);

INCLUDE_ASM("actors/nonmatchings/actor_151000/actor_151000", func_actor_151000_8013288C);

INCLUDE_ASM("actors/nonmatchings/actor_151000/actor_151000", func_actor_151000_801328DC);

INCLUDE_ASM("actors/nonmatchings/actor_151000/actor_151000", func_actor_151000_80132A38);

INCLUDE_RODATA("actors/nonmatchings/actor_151000/actor_151000", D_actor_151000_80131E20);
