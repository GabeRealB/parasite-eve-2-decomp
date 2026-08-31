#include "common.h"
#include "main/task.h"
extern TaskDesc D_actor_142900_80137600;
extern s32      D_actor_142900_801382AC;

INCLUDE_ASM("actors/nonmatchings/actor_142900/actor_142900", func_actor_142900_80131E24);

INCLUDE_ASM("actors/nonmatchings/actor_142900/actor_142900", func_actor_142900_80131F5C);

void func_actor_142900_80131FDC(s32 arg0)
{
    if (arg0 == 1) {
        Task_SpawnFromTable(&D_actor_142900_80137600, 1, 0, 0);
    }
    D_actor_142900_801382AC = arg0;
}

INCLUDE_RODATA("actors/nonmatchings/actor_142900/actor_142900", D_actor_142900_80131E20);
