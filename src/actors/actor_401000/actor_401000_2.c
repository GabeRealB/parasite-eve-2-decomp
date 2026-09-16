#include "common.h"

#include "actors/actor_401000.h"

INCLUDE_ASM("actors/nonmatchings/actor_401000/actor_401000_2", func_actor_401000_8013D958);

void func_actor_401000_8013DA78(Task* task)
{
    Actor401000Work* work;
    GpEnemy*         enemy;

    work  = (Actor401000Work*)task->idMap;
    enemy = (GpEnemy*)task->spawnArg2;
    if (work != NULL) {
        if (work->field_C1C != NULL) {
            Task_Kill(work->field_C1C);
        }
        if (work->field_C20 != NULL) {
            Task_Kill(work->field_C20);
        }
        Gp_UnlinkObj(&work->field_B50);
        Gp_UnlinkObj(&work->field_8D0);
        Gp_UnlinkObj(&work->field_A10);
        enemy->field_54 = 0;
    }
    Gp_DestroyEnemy(enemy, task);
}

void func_actor_401000_8013DB10(Actor401000* arg0)
{
    TmdObject*       obj;
    Actor401000Work* work;

    work = arg0->field_1C;
    if (work->field_4 != 0) {
        obj                          = arg0->field_2C;
        arg0->field_20->node.field_4 = 1;
        obj->field_C                 = (u16)(obj->field_C | 0x80);
        work->field_B50.flags        = (u16)(work->field_B50.flags & 0x7FFF);
        work->field_A10.flags        = (u16)(work->field_A10.flags | 0x4000);
    }
}

void func_actor_401000_8013DB6C(Actor401000* arg0)
{
    TmdObject*       obj;
    Actor401000Work* work;

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
        work->field_B50.flags = (u16)(work->field_B50.flags & 0x7FFF);
        work->field_A10.flags = (u16)(work->field_A10.flags | 0x4000);
        func_actor_401000_80132EF0(arg0);
    } else {
        arg0->field_2C->field_8->flg = 0;
        func_actor_401000_80132EF0(arg0);
    }
}

void func_actor_401000_8013DC14(Actor401000* arg0)
{
    TmdObject*       obj;
    Actor401000Work* work;

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
        work->field_B50.flags = (u16)(work->field_B50.flags & 0x7FFF);
        work->field_A10.flags = (u16)(work->field_A10.flags | 0x4000);
        func_actor_401000_80132EF0(arg0);
    } else {
        arg0->field_2C->field_8->flg = 0;
        func_actor_401000_80132EF0(arg0);
    }
}

void func_actor_401000_8013DCC0(Actor401000* arg0)
{
    TmdObject*       obj;
    Actor401000Work* work;

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
        work->field_B50.flags = (u16)(work->field_B50.flags & 0x7FFF);
        work->field_A10.flags = (u16)(work->field_A10.flags | 0x4000);
        func_actor_401000_80132EF0(arg0);
    } else {
        arg0->field_2C->field_8->flg = 0;
        func_actor_401000_80132EF0(arg0);
    }
}

void func_actor_401000_8013DD6C(Actor401000* arg0)
{
    TmdObject*       obj;
    Actor401000Work* work;

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
        work->field_B50.flags = (u16)(work->field_B50.flags & 0x7FFF);
        work->field_A10.flags = (u16)(work->field_A10.flags | 0x4000);
    }
    arg0->field_2C->field_8->flg = 0;
    func_actor_401000_80132EF0(arg0);
    if (work->field_68 & 1) {
        work->field_0 = 7;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_401000/actor_401000_2", func_actor_401000_8013DE24);

INCLUDE_ASM("actors/nonmatchings/actor_401000/actor_401000_2", func_actor_401000_8013DEC8);

INCLUDE_ASM("actors/nonmatchings/actor_401000/actor_401000_2", func_actor_401000_8013DF6C);
