#include "common.h"

#include "actors/actor_356100.h"

INCLUDE_RODATA("actors/nonmatchings/actor_356100/actor_356100_2", ActorsShared80135df4Table);

INCLUDE_ASM("actors/nonmatchings/actor_356100/actor_356100_2", func_actor_356100_8016A0B8);

void func_actor_356100_8016A158(Task* task)
{
    Actor356100Work* work;
    GpEnemy*         enemy;

    work  = (Actor356100Work*)task->idMap;
    enemy = (GpEnemy*)task->spawnArg2;
    if (work != NULL) {
        if (work->field_B5C != NULL) {
            Task_Kill(work->field_B5C);
        }
        if (work->field_B60 != NULL) {
            Task_Kill(work->field_B60);
        }
        enemy->field_54 = 0;
    }
    Gp_DestroyEnemy(enemy, task);
}

void func_actor_356100_8016A1D8(Actor356100* arg0)
{
    TmdObject*       obj;
    Actor356100Work* work;

    work = arg0->field_1C;
    if (work->field_4 != 0) {
        obj                          = arg0->field_2C;
        arg0->field_20->node.field_4 = 1;
        obj->field_C                |= 0x80;
    }
}

void func_actor_356100_8016A21C(Actor356100* arg0)
{
    TmdObject*       obj;
    Actor356100Work* work;

    work = arg0->field_1C;
    if (work->field_4 != 0) {
        obj                          = arg0->field_2C;
        arg0->field_20->node.field_4 = 0;
        obj->field_C                 = 0;
        Tmd_AllocBuffers(obj);
        work->field_978 = 2;
        work->field_97A = 0;
        work->field_982 = 0x10;
        work->field_97E = 2;
        func_actor_356100_80163508(arg0);
    } else {
        arg0->field_2C->field_8->flg = 0;
        func_actor_356100_80163508(arg0);
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_356100/actor_356100_2", func_actor_356100_8016A2AC);

void func_actor_356100_8016A340(Actor356100* arg0)
{
    TmdObject*       obj;
    Actor356100Work* work;

    work = arg0->field_1C;
    if (work->field_4 != 0) {
        obj                          = arg0->field_2C;
        arg0->field_20->node.field_4 = 0;
        obj->field_C                 = 0;
        Tmd_AllocBuffers(obj);
        work->field_978 = 2;
        work->field_982 = 0x10;
        work->field_97A = 0;
        work->field_97E = 0xB;
        func_actor_356100_80163508(arg0);
    } else {
        arg0->field_2C->field_8->flg = 0;
        func_actor_356100_80163508(arg0);
    }
}

void func_actor_356100_8016A3D4(Actor356100* arg0)
{
    TmdObject*       obj;
    Actor356100Work* work;

    work = arg0->field_1C;
    if (work->field_4 != 0) {
        obj                          = arg0->field_2C;
        arg0->field_20->node.field_4 = 0;
        obj->field_C                 = 0;
        Tmd_AllocBuffers(obj);
        work->field_978 = 2;
        work->field_982 = 0x10;
        work->field_97A = 0;
        work->field_97E = 0xB;
        func_actor_356100_80163508(arg0);
    } else {
        arg0->field_2C->field_8->flg = 0;
        func_actor_356100_80163508(arg0);
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_356100/actor_356100_2", func_actor_356100_8016A468);

INCLUDE_ASM("actors/nonmatchings/actor_356100/actor_356100_2", func_actor_356100_8016A550);

INCLUDE_ASM("actors/nonmatchings/actor_356100/actor_356100_2", func_actor_356100_8016A5DC);

INCLUDE_ASM("actors/nonmatchings/actor_356100/actor_356100_2", func_actor_356100_8016A668);

INCLUDE_ASM("actors/nonmatchings/actor_356100/actor_356100_2", func_actor_356100_8016A710);

INCLUDE_ASM("actors/nonmatchings/actor_356100/actor_356100_2", func_actor_356100_8016A834);
