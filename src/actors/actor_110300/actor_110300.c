#include "common.h"

#include "actors/actor_110300.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"

/// Step 0 of the `ActorsShared80131f9c` dispatcher: allocate the work block,
/// publish it, and hand the model's animation context its slot array.
///
/// Every access to the block goes through `ActorsShared80131f9cWork` rather
/// than the `memCalloc` result, which is why the pointer is reloaded at each
/// use instead of staying in a callee-saved register. `task->msgTable` takes
/// the message table the step-1 handler leaves behind.
void ActorsShared80131f9cSub0(GpEnemy* enemy, Task* task)
{
    VECTOR         vec;
    void*          work;
    TmdObject*     obj;
    GsCOORDINATE2* coord;

    obj                      = task->extra;
    coord                    = obj->coords;
    work                     = memCalloc(0x55C, 0);
    ActorsShared80131f9cWork = work;
    task->work               = work;
    if (work == NULL) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }
    task->exitCallback      = func_actor_110300_80132088;
    coord->sub              = &gGfxViewCoord;
    enemy->field_4          = &coord->coord;
    enemy->node.flags       = 1;
    enemy->field_48         = 0;
    enemy->node.targeted    = 0;
    obj->otOffset           = 0;
    coord->flg              = 0;
    D_actor_110300_8013A0A4 = (GpActorWork*)task;
    D_actor_110300_8013A0A8 = Task_SpawnFromTable(D_actor_110300_8013A06C, 1, 0, 0);
    func_800B3F84(&ActorsShared80131f9cWork->anim, D_actor_110300_8013A084, obj,
                  ActorsShared80131f9cWork->aux, ActorsShared80131f9cWork->slots);
    ActorsShared80131f9cWork->animId    = 1;
    ActorsShared80131f9cWork->field_474 = 2;
    func_actor_110300_801320C4((GpActorWork*)task);
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1];
    vec.vz = coord->workm.t[2];
    func_800D7A9C(obj, &vec, 0, 3);
    ActorsShared80131f9cWork->field_47A++;
    task->msgTable = D_actor_110300_8013A054;
    task->state++;
}

INCLUDE_RODATA("actors/nonmatchings/actor_110300/actor_110300", D_actor_110300_80131E20);
