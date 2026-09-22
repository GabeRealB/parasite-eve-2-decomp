#include "common.h"

#include "actors/actors_shared_80164c20.h"

/// When the work block's `field_4` flag is set, flags the enemy's link node and
/// raises bit 0x80 of the model's `field_C`. `obj` gets its own local for the
/// same reason as `func_actor_206100_8014FAE4`: the fused form ranks the
/// `Task::extra` load with the store and transposes it.
void ActorsShared80164c20(GpEnemy* arg0, Task* arg1)
{
    ActorsShared80164c20Work* work;
    TmdObject*                obj;

    work = (ActorsShared80164c20Work*)arg1->work;
    if (work->field_4 != 0) {
        obj              = (TmdObject*)arg1->extra;
        arg0->node.flags = 1;
        obj->flags      |= 0x80;
    }
}
