#include "common.h"

#include "actors/actor_335800.h"

#include "gameplay/gameplay.h"

#include "main/task.h"

#include "main/tmd.h"

INCLUDE_ASM("actors/nonmatchings/actor_335800/actor_335800_4", func_actor_335800_801631A4);

INCLUDE_ASM("actors/nonmatchings/actor_335800/actor_335800_4", func_actor_335800_801632A4);

INCLUDE_ASM("actors/nonmatchings/actor_335800/actor_335800_4", func_actor_335800_801633C0);

INCLUDE_ASM("actors/nonmatchings/actor_335800/actor_335800_4", func_actor_335800_8016343C);

s32 func_actor_335800_8016354C(Task* arg0, s32 arg1, Actor335800Msg* arg2, s32 arg3)
{
    Actor335800MainWork* work;

    work = (Actor335800MainWork*)arg0->idMap;
    if (arg2->field_2 == 0) {
        work->field_504 = 0;
    }
    return 0;
}

INCLUDE_ASM("actors/nonmatchings/actor_335800/actor_335800_4", func_actor_335800_80163568);

INCLUDE_ASM("actors/nonmatchings/actor_335800/actor_335800_4", func_actor_335800_8016373C);

INCLUDE_ASM("actors/nonmatchings/actor_335800/actor_335800_4", func_actor_335800_80163880);

INCLUDE_ASM("actors/nonmatchings/actor_335800/actor_335800_4", func_actor_335800_80163A34);

INCLUDE_ASM("actors/nonmatchings/actor_335800/actor_335800_4", func_actor_335800_80163AA0);

void func_actor_335800_80163B34(Task* arg0)
{
    Gp_EnemyTaskExit(arg0);
}

void func_actor_335800_80163B54(Task* arg0)
{
    TmdObject*       ext;
    Actor335800Work* work;

    ext           = arg0->extra;
    work          = (Actor335800Work*)arg0->idMap;
    ext->field_1C = &work->light;
    ext->field_20 = &work->color;
}

void func_actor_335800_80163B70(void)
{
}

INCLUDE_ASM("actors/nonmatchings/actor_335800/actor_335800_4", func_actor_335800_80163B78);

INCLUDE_ASM("actors/nonmatchings/actor_335800/actor_335800_4", func_actor_335800_80163BE0);

INCLUDE_ASM("actors/nonmatchings/actor_335800/actor_335800_4", func_actor_335800_80163CA0);

INCLUDE_ASM("actors/nonmatchings/actor_335800/actor_335800_4", func_actor_335800_80163D20);

INCLUDE_ASM("actors/nonmatchings/actor_335800/actor_335800_4", func_actor_335800_80163E20);

INCLUDE_ASM("actors/nonmatchings/actor_335800/actor_335800_4", func_actor_335800_80163F3C);

INCLUDE_ASM("actors/nonmatchings/actor_335800/actor_335800_4", func_actor_335800_80163FB8);

s32 func_actor_335800_80164098(void)
{
    return 0;
}
