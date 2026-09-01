#include "common.h"

#include "actors/actor_350500.h"

#include "main/task.h"
#include "main/tmd.h"

void func_actor_350500_8016247C(Task* arg0)
{
    TmdObject*       ext;
    Actor350500Work* work;

    ext           = arg0->extra;
    work          = (Actor350500Work*)arg0->idMap;
    ext->field_1C = &work->light;
    ext->field_20 = &work->color;
}

void func_actor_350500_80162498(void)
{
}

INCLUDE_ASM("actors/nonmatchings/actor_350500/actor_350500_2", func_actor_350500_801624A0);

INCLUDE_ASM("actors/nonmatchings/actor_350500/actor_350500_2", func_actor_350500_80162508);

INCLUDE_ASM("actors/nonmatchings/actor_350500/actor_350500_2", func_actor_350500_801625E4);

INCLUDE_ASM("actors/nonmatchings/actor_350500/actor_350500_2", func_actor_350500_8016272C);

INCLUDE_ASM("actors/nonmatchings/actor_350500/actor_350500_2", func_actor_350500_80162828);

INCLUDE_ASM("actors/nonmatchings/actor_350500/actor_350500_2", func_actor_350500_80162960);

INCLUDE_ASM("actors/nonmatchings/actor_350500/actor_350500_2", func_actor_350500_801629DC);

INCLUDE_ASM("actors/nonmatchings/actor_350500/actor_350500_2", func_actor_350500_80162ABC);
