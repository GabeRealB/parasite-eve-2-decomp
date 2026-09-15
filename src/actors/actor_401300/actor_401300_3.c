#include "common.h"

#include "actors/actor_401300.h"

void func_actor_401300_80141758(Task* task)
{
    Actor401300Work* work;
    GpEnemy*         enemy;

    work  = (Actor401300Work*)task->idMap;
    enemy = (GpEnemy*)task->spawnArg2;
    if (work != NULL) {
        if (work->field_D0C != NULL) {
            Task_Kill(work->field_D0C);
        }
        if (work->field_D10 != NULL) {
            Task_Kill(work->field_D10);
        }
        Gp_UnlinkObj(&work->field_BF0);
        Gp_UnlinkObj(&work->field_970);
        Gp_UnlinkObj(&work->field_AB0);
        enemy->field_54 = 0;
    }
    Gp_DestroyEnemy(enemy, task);
}

INCLUDE_ASM("actors/nonmatchings/actor_401300/actor_401300_3", func_actor_401300_801417F0);

INCLUDE_ASM("actors/nonmatchings/actor_401300/actor_401300_3", func_actor_401300_8014192C);

void func_actor_401300_801419B8(Actor401300* arg0)
{
    TmdObject*       obj;
    Actor401300Work* work;

    work = arg0->field_1C;
    if (work->field_4 != 0) {
        obj                          = arg0->field_2C;
        arg0->field_20->node.field_4 = 0;
        obj->field_C                 = 0;
        Tmd_AllocBuffers(obj);
        work->field_89C       = 2;
        work->field_8A6       = 0x10;
        work->field_8A2       = 2;
        work->field_89E       = 0;
        work->field_BF0.flags = (u16)(work->field_BF0.flags & 0x7FFF);
        work->field_AB0.flags = (u16)(work->field_AB0.flags & 0xBFFF);
        func_actor_401300_80133A3C(arg0);
    } else {
        arg0->field_2C->field_8->flg = 0;
        func_actor_401300_80133A3C(arg0);
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_401300/actor_401300_3", func_actor_401300_80141A60);

INCLUDE_ASM("actors/nonmatchings/actor_401300/actor_401300_3", func_actor_401300_80141B0C);

INCLUDE_ASM("actors/nonmatchings/actor_401300/actor_401300_3", func_actor_401300_80141BC8);

void func_actor_401300_80141C80(void)
{
}

INCLUDE_ASM("actors/nonmatchings/actor_401300/actor_401300_3", func_actor_401300_80141C88);

INCLUDE_ASM("actors/nonmatchings/actor_401300/actor_401300_3", func_actor_401300_80141D50);

INCLUDE_ASM("actors/nonmatchings/actor_401300/actor_401300_3", func_actor_401300_80141DF4);

void func_actor_401300_80141EF8(Task* task)
{
    Actor401300Work* work  = (Actor401300Work*)task->idMap;
    GpEnemy*         enemy = task->spawnArg2;

    if (enemy->field_40 != -0x3E7 && work->field_C8A == 0) {
        enemy->field_40 = -0x3E7;
    }
}
