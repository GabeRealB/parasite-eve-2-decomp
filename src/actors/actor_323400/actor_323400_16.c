#include "common.h"

#include "actors/actor_323400.h"
#include "gameplay/1BC.h"
#include "main/tmd.h"

/// State 0 of `D_actor_323400_80161E24`: when the work block's `field_4` flag
/// is set, flags the enemy's link node and hides the model (raises flag
/// 0x80). `obj` gets its own local: the fused form ranks the `Task::extra`
/// load with the store and transposes it.
void func_actor_323400_80164B98(GpEnemy* arg0, Task* arg1)
{
    Actor323400Work* work;
    TmdObject*       obj;

    work = (Actor323400Work*)arg1->work;
    if (work->field_4 != 0) {
        obj              = (TmdObject*)arg1->extra;
        arg0->node.flags = 1;
        obj->flags      |= 0x80;
    }
}
