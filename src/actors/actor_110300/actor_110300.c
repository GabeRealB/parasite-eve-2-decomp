#include "common.h"

#include "actors/actor_110300.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"

/// Step 0 of the `func_actor_110300_80131F9C` dispatcher: allocate the work
/// block, publish it, and hand the model's animation context its slot array.
///
/// Every access to the block goes through `D_actor_110300_8013A0A0` rather
/// than the `memCalloc` result, which is why the pointer is reloaded at each
/// use instead of staying in a callee-saved register. The task's message table
/// becomes the one holding the animation-start and visibility handlers.
void func_actor_110300_80131E24(GpEnemy* enemy, Task* task)
{
    VECTOR         vec;
    void*          work;
    TmdObject*     obj;
    GsCOORDINATE2* coord;

    obj                     = task->extra;
    coord                   = obj->coords;
    work                    = memCalloc(sizeof(Actor110300Work), 0);
    D_actor_110300_8013A0A0 = work;
    task->work              = work;
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
    func_800B3F84(&D_actor_110300_8013A0A0->anim, D_actor_110300_8013A084, obj,
                  D_actor_110300_8013A0A0->aux, D_actor_110300_8013A0A0->slots);
    D_actor_110300_8013A0A0->animId    = 1;
    D_actor_110300_8013A0A0->field_474 = 2;
    func_actor_110300_801320C4((GpActorWork*)task);
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1];
    vec.vz = coord->workm.t[2];
    func_800D7A9C(obj, &vec, 0, 3);
    D_actor_110300_8013A0A0->field_47A++;
    task->msgTable = D_actor_110300_8013A054;
    task->state++;
}
