#include "common.h"

#include "actors/actor_323300.h"

INCLUDE_ASM("actors/nonmatchings/actor_323300/actor_323300", func_actor_323300_80161E78);

INCLUDE_ASM("actors/nonmatchings/actor_323300/actor_323300", func_actor_323300_80161FE8);

INCLUDE_ASM("actors/nonmatchings/actor_323300/actor_323300", func_actor_323300_80162208);

INCLUDE_RODATA("actors/nonmatchings/actor_323300/actor_323300", D_actor_323300_80161E20);

INCLUDE_RODATA("actors/nonmatchings/actor_323300/actor_323300", D_actor_323300_80161E24);

INCLUDE_ASM("actors/nonmatchings/actor_323300/actor_323300", func_actor_323300_80162360);

INCLUDE_ASM("actors/nonmatchings/actor_323300/actor_323300", func_actor_323300_80162630);

void func_actor_323300_8016269C(Task* arg0)
{
    Gp_UnlinkObj(&((Actor323300Work*)arg0->idMap)->obj);
    Gp_EnemyTaskExit(arg0);
}

void func_actor_323300_801626D0(Task* arg0)
{
    TmdObject*       ext;
    Actor323300Work* work;

    ext           = arg0->extra;
    work          = (Actor323300Work*)arg0->idMap;
    ext->field_1C = &work->light;
    ext->field_20 = &work->color;
}

void func_actor_323300_801626EC(void)
{
}

INCLUDE_ASM("actors/nonmatchings/actor_323300/actor_323300", func_actor_323300_801626F4);

INCLUDE_ASM("actors/nonmatchings/actor_323300/actor_323300", func_actor_323300_80162748);

INCLUDE_ASM("actors/nonmatchings/actor_323300/actor_323300", func_actor_323300_801627B4);

INCLUDE_ASM("actors/nonmatchings/actor_323300/actor_323300", func_actor_323300_801628B8);

INCLUDE_ASM("actors/nonmatchings/actor_323300/actor_323300", func_actor_323300_801629F0);

INCLUDE_ASM("actors/nonmatchings/actor_323300/actor_323300", func_actor_323300_80162A6C);

INCLUDE_ASM("actors/nonmatchings/actor_323300/actor_323300", func_actor_323300_80162BE4);

INCLUDE_ASM("actors/nonmatchings/actor_323300/actor_323300", func_actor_323300_80162DF0);

INCLUDE_ASM("actors/nonmatchings/actor_323300/actor_323300", func_actor_323300_80163188);

INCLUDE_ASM("actors/nonmatchings/actor_323300/actor_323300", func_actor_323300_801634B0);

INCLUDE_ASM("actors/nonmatchings/actor_323300/actor_323300", func_actor_323300_80163510);

INCLUDE_ASM("actors/nonmatchings/actor_323300/actor_323300", func_actor_323300_8016359C);

INCLUDE_ASM("actors/nonmatchings/actor_323300/actor_323300", func_actor_323300_8016369C);

INCLUDE_ASM("actors/nonmatchings/actor_323300/actor_323300", func_actor_323300_80163718);

INCLUDE_ASM("actors/nonmatchings/actor_323300/actor_323300", func_actor_323300_80163840);
