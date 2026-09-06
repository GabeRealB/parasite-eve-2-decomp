#include "common.h"

#include "main/task.h"
#include "main/tmd.h"

#include "actors/actor_342000.h"

INCLUDE_ASM("actors/nonmatchings/actor_342000/actor_342000", func_actor_342000_80161EA4);

INCLUDE_ASM("actors/nonmatchings/actor_342000/actor_342000", func_actor_342000_8016201C);

INCLUDE_ASM("actors/nonmatchings/actor_342000/actor_342000", func_actor_342000_80162158);

INCLUDE_ASM("actors/nonmatchings/actor_342000/actor_342000", func_actor_342000_801625D8);

INCLUDE_ASM("actors/nonmatchings/actor_342000/actor_342000", func_actor_342000_801628C8);

INCLUDE_RODATA("actors/nonmatchings/actor_342000/actor_342000", D_actor_342000_80161E20);

INCLUDE_ASM("actors/nonmatchings/actor_342000/actor_342000", func_actor_342000_80162BBC);

INCLUDE_ASM("actors/nonmatchings/actor_342000/actor_342000", func_actor_342000_80162F28);

INCLUDE_ASM("actors/nonmatchings/actor_342000/actor_342000", func_actor_342000_8016382C);

INCLUDE_ASM("actors/nonmatchings/actor_342000/actor_342000", func_actor_342000_80163EAC);

void func_actor_342000_80163F88(Task* task)
{
    Actor342000Work* work;
    GsCOORDINATE2*   coord;

    coord = (GsCOORDINATE2*)((TmdObject*)task->extra)->field_8;
    work  = (Actor342000Work*)task->idMap;

    coord->sub = work->field_2A4;
    Task_Kill(task);
}

INCLUDE_ASM("actors/nonmatchings/actor_342000/actor_342000", func_actor_342000_80163FB8);
