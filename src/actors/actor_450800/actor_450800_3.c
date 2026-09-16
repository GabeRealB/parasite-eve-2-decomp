#include "common.h"
#include "main/task.h"
#include "main/tmd.h"

#include "actors/actor_450800.h"
#include "gameplay/3CD8.h"

/// Message handler 0x7DB of `D_actor_450800_8014AC58`: recolour this actor's
/// body (or spawn its 0x6002B burst) according to the message's selector.
///
/// The model is the actor's own -- `task->extra`, the `TmdObject` a spawnType-1
/// task carries -- and the one it is driven through is that of the helper task
/// in `Actor450800Work::field_4F8`. Both pointers, and `field_8` of the helper's
/// model, are resolved before the switch: the ROM reads them there, and a
/// scheduler pass cannot lift the loads into the entry block on its own.
s32 func_actor_450800_80132CE0(Task* task, s32 arg1, Actor450800Msg* msg, s32 arg3)
{
    Actor450800Work* work  = (Actor450800Work*)task->idMap;
    TmdObject*       obj   = (TmdObject*)work->field_4F8->extra;
    GsCOORDINATE2*   coord = obj->field_8;
    TmdObject*       self  = (TmdObject*)task->extra;
    s32              mode  = msg->field_2;

    switch (mode) {
        case 0:
            Gp_SpawnEff(0x6002B, coord, 0x21, 0);
            break;
        case 1:
            work->field_500 = mode;
            obj->field_C    = self->field_C;
            break;
        case 2:
            work->field_500 = 0;
            obj->field_C    = 0x84;
            break;
    }
    return 0;
}

INCLUDE_ASM("actors/nonmatchings/actor_450800/actor_450800_3", func_actor_450800_80132D74);

INCLUDE_ASM("actors/nonmatchings/actor_450800/actor_450800_3", func_actor_450800_80132E9C);

INCLUDE_ASM("actors/nonmatchings/actor_450800/actor_450800_3", func_actor_450800_801330AC);

void func_actor_450800_80132E9C(void* enemy, Task* task);
void func_actor_450800_801332B8(void* enemy, Task* task);

void func_actor_450800_80133264(Task* task)
{
    void (*fns[2])(void*, Task*) = { func_actor_450800_80132E9C, func_actor_450800_801332B8 };

    fns[task->state](task->spawnArg2, task);
}

INCLUDE_ASM("actors/nonmatchings/actor_450800/actor_450800_3", func_actor_450800_801332B8);
