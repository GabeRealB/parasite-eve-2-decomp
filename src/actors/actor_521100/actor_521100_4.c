#include "common.h"

#include "actors/actor_521100.h"

/// Main-executable global with no module header yet: the remaining-enemy count.
extern s16 D_80073BA0;

INCLUDE_ASM("actors/nonmatchings/actor_521100/actor_521100_4", func_actor_521100_80135B40);

void func_actor_521100_80135B80(GpEnemy* arg0, Task* task)
{
    Actor521100Obj2C* obj;
    Actor521100Work*  work;
    s16               mode;

    work = (Actor521100Work*)task->parent->idMap;
    obj  = (Actor521100Obj2C*)task->extra;
    if (work->field_682 != 0) {
        mode         = ((work->field_692 & 1) == 0) << 7;
        obj->field_C = mode;
        if (work->field_692 & 2) {
            obj->field_C = mode | 4;
        }
        if (work->field_694 != 0) {
            obj->field_C = 0x80;
        }
    }
}

s32 func_actor_521100_80135BEC(Actor521100* arg0)
{
    if (D_80073BA0 > 0) {
        arg0->field_1C->field_6A8 = 1;
    }
    return 0;
}

INCLUDE_ASM("actors/nonmatchings/actor_521100/actor_521100_4", func_actor_521100_80135C14);
