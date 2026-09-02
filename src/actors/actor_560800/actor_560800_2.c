#include "common.h"
#include "main/sound.h"
#include "main/task.h"

extern TaskDesc ActorsShared80136280Desc;
extern s32      D_actor_560800_8016F57C[];

void func_actor_560800_801362B0(s32 arg0)
{
    Task_SpawnFromTable(&ActorsShared80136280Desc, 3, arg0, 0);
}

INCLUDE_ASM("actors/nonmatchings/actor_560800/actor_560800_2", func_actor_560800_801362E0);

INCLUDE_ASM("actors/nonmatchings/actor_560800/actor_560800_2", func_actor_560800_8013631C);

INCLUDE_ASM("actors/nonmatchings/actor_560800/actor_560800_2", func_actor_560800_80136358);

INCLUDE_ASM("actors/nonmatchings/actor_560800/actor_560800_2", func_actor_560800_80136378);

INCLUDE_ASM("actors/nonmatchings/actor_560800/actor_560800_2", func_actor_560800_801363F8);

INCLUDE_ASM("actors/nonmatchings/actor_560800/actor_560800_2", func_actor_560800_801364A0);

INCLUDE_ASM("actors/nonmatchings/actor_560800/actor_560800_2", func_actor_560800_80136548);

INCLUDE_ASM("actors/nonmatchings/actor_560800/actor_560800_2", func_actor_560800_801365B0);

INCLUDE_ASM("actors/nonmatchings/actor_560800/actor_560800_2", func_actor_560800_801365D0);

void func_actor_560800_80136678(s32 arg0)
{
    SndEvt_EnqueueType6(D_actor_560800_8016F57C[arg0], 0, 0);
}

INCLUDE_ASM("actors/nonmatchings/actor_560800/actor_560800_2", func_actor_560800_801366B0);

INCLUDE_ASM("actors/nonmatchings/actor_560800/actor_560800_2", func_actor_560800_801367C0);

INCLUDE_ASM("actors/nonmatchings/actor_560800/actor_560800_2", func_actor_560800_801367E0);

INCLUDE_ASM("actors/nonmatchings/actor_560800/actor_560800_2", func_actor_560800_80136818);

INCLUDE_ASM("actors/nonmatchings/actor_560800/actor_560800_2", func_actor_560800_80136878);
