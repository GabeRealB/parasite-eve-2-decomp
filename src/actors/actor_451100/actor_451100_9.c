#include "common.h"

#include "actors/actor_451100.h"
#include "main/tmd.h"

/// Per-frame handler of one of this actor's sub-model tasks: the same body as
/// the shared `ActorsShared801328bc`, reached with the sub-model's own
/// `TmdObject` in `Task::extra` and the actor holding it as `Task::parent`.
/// This copy aims the part's `sub` at the ninth trailing coordinate of the
/// parent's model (`field_8[8]`) where the shared body uses the eighth, which
/// is why the shared body's two other users cannot carry it.
void func_actor_451100_801330B0(Task* task)
{
    char           pad[0x10];
    Task*          parent = task->parent;
    TmdObject*     obj    = task->extra;
    GsCOORDINATE2* coord  = obj->field_8;
    GsCOORDINATE2* sub    = &((TmdObject*)parent->extra)->field_8[8];
    MATRIX*        work   = (MATRIX*)parent->work;

    switch (task->state) {
        case 0:
            coord->flg    = 0;
            obj->field_1C = work;
            obj->field_20 = work + 1;
            coord->sub    = sub;
            task->state++;
            break;
        case 1:
            coord->flg = 0;
            break;
    }
}
