#include "common.h"

#include "actors/actor_401800.h"

INCLUDE_ASM("actors/nonmatchings/actor_401800/actor_401800_2", func_actor_401800_8013DF80);

void func_actor_401800_8013E0A0(Task* task)
{
    Actor401800Work* work;
    GpEnemy*         enemy;

    work  = (Actor401800Work*)task->idMap;
    enemy = (GpEnemy*)task->spawnArg2;
    if (work != NULL) {
        if (work->field_C14 != NULL) {
            Task_Kill(work->field_C14);
        }
        if (work->field_C18 != NULL) {
            Task_Kill(work->field_C18);
        }
        Gp_UnlinkObj(&work->field_B48);
        Gp_UnlinkObj(&work->field_8C8);
        Gp_UnlinkObj(&work->field_A08);
        enemy->field_54 = 0;
    }
    Gp_DestroyEnemy(enemy, task);
}

void func_actor_401800_8013E138(Actor401800* arg0)
{
    TmdObject*       obj;
    Actor401800Work* work;

    work = arg0->field_1C;
    if (work->field_4 != 0) {
        obj                          = arg0->field_2C;
        arg0->field_20->node.field_4 = 1;
        obj->field_C                 = (u16)(obj->field_C | 0x80);
        work->field_B48.flags        = (u16)(work->field_B48.flags & 0x7FFF);
        work->field_A08.flags        = (u16)(work->field_A08.flags & 0xBFFF);
    }
}

void func_actor_401800_8013E194(Actor401800* arg0)
{
    TmdObject*       obj;
    Actor401800Work* work;

    work = arg0->field_1C;
    if (work->field_4 != 0) {
        obj                          = arg0->field_2C;
        arg0->field_20->node.field_4 = 0;
        obj->field_C                 = 0;
        Tmd_AllocBuffers(obj);
        work->field_898       = 2;
        work->field_8A2       = 0x10;
        work->field_89E       = 2;
        work->field_89A       = 0;
        work->field_B48.flags = (u16)(work->field_B48.flags & 0x7FFF);
        work->field_A08.flags = (u16)(work->field_A08.flags & 0xBFFF);
        func_actor_401800_80133EB8(arg0);
    } else {
        arg0->field_2C->field_8->flg = 0;
        func_actor_401800_80133EB8(arg0);
    }
}

void func_actor_401800_8013E23C(Actor401800* arg0)
{
    TmdObject*       obj;
    Actor401800Work* work;

    work = arg0->field_1C;
    if (work->field_4 != 0) {
        obj                          = arg0->field_2C;
        arg0->field_20->node.field_4 = 0;
        obj->field_C                 = 0;
        Tmd_AllocBuffers(obj);
        work->field_898       = 2;
        work->field_8A2       = 0x10;
        work->field_89E       = 3;
        work->field_89A       = 0;
        work->field_B48.flags = (u16)(work->field_B48.flags & 0x7FFF);
        work->field_A08.flags = (u16)(work->field_A08.flags & 0xBFFF);
        func_actor_401800_80133EB8(arg0);
    } else {
        arg0->field_2C->field_8->flg = 0;
        func_actor_401800_80133EB8(arg0);
    }
}

void func_actor_401800_8013E2E8(Actor401800* arg0)
{
    TmdObject*       obj;
    Actor401800Work* work;

    work = arg0->field_1C;
    if (work->field_4 != 0) {
        obj                          = arg0->field_2C;
        arg0->field_20->node.field_4 = 0;
        obj->field_C                 = 0;
        Tmd_AllocBuffers(obj);
        work->field_898       = 2;
        work->field_8A2       = 0x10;
        work->field_89E       = 0xB;
        work->field_89A       = 0;
        work->field_B48.flags = (u16)(work->field_B48.flags & 0x7FFF);
        work->field_A08.flags = (u16)(work->field_A08.flags & 0xBFFF);
        func_actor_401800_80133EB8(arg0);
    } else {
        arg0->field_2C->field_8->flg = 0;
        func_actor_401800_80133EB8(arg0);
    }
}

void func_actor_401800_8013E394(Actor401800* arg0)
{
    TmdObject*       obj;
    Actor401800Work* work;

    work = arg0->field_1C;
    if (work->field_4 != 0) {
        obj                          = arg0->field_2C;
        arg0->field_20->node.field_4 = 0;
        obj->field_C                 = 0;
        Tmd_AllocBuffers(obj);
        work->field_898       = 2;
        work->field_8A2       = 0x12;
        work->field_89E       = 0xD;
        work->field_89A       = 0;
        work->field_B48.flags = (u16)(work->field_B48.flags & 0x7FFF);
        work->field_A08.flags = (u16)(work->field_A08.flags & 0xBFFF);
    }
    arg0->field_2C->field_8->flg = 0;
    func_actor_401800_80133EB8(arg0);
    if (work->field_68 & 1) {
        work->field_0 = 7;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_401800/actor_401800_2", func_actor_401800_8013E44C);

INCLUDE_ASM("actors/nonmatchings/actor_401800/actor_401800_2", func_actor_401800_8013E4F0);

INCLUDE_ASM("actors/nonmatchings/actor_401800/actor_401800_2", func_actor_401800_8013E5A4);
