#include "common.h"

#include "actors/actor_141000.h"

#include "main/task.h"
#include "main/tmd.h"

void func_actor_141000_801339DC(Task* arg0)
{
    TmdObject*       ext;
    Actor141000Work* work;

    ext           = arg0->extra;
    work          = (Actor141000Work*)arg0->idMap;
    ext->field_1C = &work->light;
    ext->field_20 = &work->color;
}

void func_actor_141000_801339F8(void)
{
}

INCLUDE_ASM("actors/nonmatchings/actor_141000/actor_141000_3", func_actor_141000_80133A00);

INCLUDE_ASM("actors/nonmatchings/actor_141000/actor_141000_3", func_actor_141000_80133A68);

INCLUDE_ASM("actors/nonmatchings/actor_141000/actor_141000_3", func_actor_141000_80133B28);

INCLUDE_ASM("actors/nonmatchings/actor_141000/actor_141000_3", func_actor_141000_80133BD8);

INCLUDE_ASM("actors/nonmatchings/actor_141000/actor_141000_3", func_actor_141000_80133CD8);

INCLUDE_ASM("actors/nonmatchings/actor_141000/actor_141000_3", func_actor_141000_80133E10);

INCLUDE_ASM("actors/nonmatchings/actor_141000/actor_141000_3", func_actor_141000_80133E8C);

INCLUDE_ASM("actors/nonmatchings/actor_141000/actor_141000_3", func_actor_141000_80133F6C);

INCLUDE_ASM("actors/nonmatchings/actor_141000/actor_141000_3", func_actor_141000_80133FA8);
