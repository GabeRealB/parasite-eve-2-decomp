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

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000_2", func_actor_403000_8013D850);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000_2", func_actor_403000_8013D910);

INCLUDE_ASM("actors/nonmatchings/actor_403000/actor_403000_2", func_actor_403000_8013D98C);
