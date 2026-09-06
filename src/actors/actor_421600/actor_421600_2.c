#include "common.h"

#include "actors/actor_421600.h"
#include "main/task.h"

s32 func_actor_421600_8013E654(Task* task)
{
    Actor421600Work* work = (Actor421600Work*)task->idMap;

    work->field_EAC = 0x1E;
    return 1;
}

INCLUDE_ASM("actors/nonmatchings/actor_421600/actor_421600_2", func_actor_421600_8013E668);

INCLUDE_ASM("actors/nonmatchings/actor_421600/actor_421600_2", func_actor_421600_8013E700);

INCLUDE_ASM("actors/nonmatchings/actor_421600/actor_421600_2", func_actor_421600_8013E7F8);

INCLUDE_ASM("actors/nonmatchings/actor_421600/actor_421600_2", func_actor_421600_8013E830);

INCLUDE_ASM("actors/nonmatchings/actor_421600/actor_421600_2", func_actor_421600_8013E858);

INCLUDE_ASM("actors/nonmatchings/actor_421600/actor_421600_2", func_actor_421600_8013E8AC);

INCLUDE_ASM("actors/nonmatchings/actor_421600/actor_421600_2", func_actor_421600_8013E9D8);

INCLUDE_ASM("actors/nonmatchings/actor_421600/actor_421600_2", func_actor_421600_8013EAAC);

INCLUDE_ASM("actors/nonmatchings/actor_421600/actor_421600_2", func_actor_421600_8013EB7C);

INCLUDE_ASM("actors/nonmatchings/actor_421600/actor_421600_2", func_actor_421600_8013EC28);

INCLUDE_ASM("actors/nonmatchings/actor_421600/actor_421600_2", func_actor_421600_8013ED24);

INCLUDE_ASM("actors/nonmatchings/actor_421600/actor_421600_2", func_actor_421600_8013EE0C);

INCLUDE_ASM("actors/nonmatchings/actor_421600/actor_421600_2", func_actor_421600_8013EEC8);
