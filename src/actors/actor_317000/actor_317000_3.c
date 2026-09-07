#include "common.h"

#include "actors/actor_317000.h"

#include "main/task.h"
#include "main/tmd.h"

void func_actor_317000_80162744(Task* arg0)
{
    TmdObject*       ext;
    Actor317000Work* work;

    ext           = arg0->extra;
    work          = (Actor317000Work*)arg0->idMap;
    ext->field_1C = &work->light;
    ext->field_20 = &work->color;
}

void func_actor_317000_80162760(void)
{
}

INCLUDE_ASM("actors/nonmatchings/actor_317000/actor_317000_3", func_actor_317000_80162768);

INCLUDE_ASM("actors/nonmatchings/actor_317000/actor_317000_3", func_actor_317000_801627D0);

INCLUDE_ASM("actors/nonmatchings/actor_317000/actor_317000_3", func_actor_317000_801628D8);

INCLUDE_ASM("actors/nonmatchings/actor_317000/actor_317000_3", func_actor_317000_80162950);

INCLUDE_ASM("actors/nonmatchings/actor_317000/actor_317000_3", func_actor_317000_80162A10);

INCLUDE_RODATA("actors/nonmatchings/actor_317000/actor_317000_3", D_actor_317000_80161E50);

INCLUDE_RODATA("actors/nonmatchings/actor_317000/actor_317000_3", D_actor_317000_80161E60);

INCLUDE_RODATA("actors/nonmatchings/actor_317000/actor_317000_3", D_actor_317000_80161E64);
