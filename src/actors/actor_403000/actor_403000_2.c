#include "common.h"

#include "actors/actor_403000.h"

void func_actor_403000_8013D5F8(Actor403000* arg0)
{
    TmdObject*       obj;
    Actor403000Work* work;
    GpEnemy*         enemy;

    work = arg0->field_1C;
    if (work->field_4 != 0) {
        obj                     = arg0->field_2C;
        enemy                   = arg0->field_20;
        work->field_FCA         = 1;
        obj->field_C           |= 0x80;
        work->objD18.obj.flags &= 0xBFFF;
        enemy->field_40         = 0;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000_2", func_actor_403000_8013D648);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000_2", func_actor_403000_8013D72C);

void func_actor_403000_8013D850(Actor403000* arg0)
{
    Actor403000Work* work;
    TmdObject*       obj;

    work = arg0->field_1C;
    if (work->field_4 != 0) {
        obj             = arg0->field_2C;
        work->field_FCA = 0;
        obj->field_C    = 0;
        Tmd_AllocBuffers(obj);
        work->objB50.obj.field_1C = 0x3E8;
        work->field_AC0           = 1;
        work->field_ACA           = 0x10;
        work->field_AC2           = 0;
        work->field_AC6           = 0x10;
        work->objD18.obj.flags   |= 0x4000;
        func_actor_403000_80133AF8(arg0);
        work->field_6 = 0;
    }
    work->field_6++;
    func_actor_403000_80133AF8(arg0);
    if (work->field_60 & 0x100) {
        work->field_FD3 = 1;
        work->field_FD2 = 1;
        work->field_0   = 2;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000_2", func_actor_403000_8013D910);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000_2", func_actor_403000_8013D98C);
