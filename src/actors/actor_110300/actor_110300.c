#include "common.h"

#include "actors/actor_110300.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"

/// State-0 handler of the `ActorsShared80131f9c` dispatcher: allocates the
/// overlay's work block, seeds the paired model from the actor's own
/// `GsCOORDINATE2`, and spawns the helper task. The three `VECTOR` components
/// have to be one address-taken local rather than three scalars - as scalars
/// only `vx` is ever read and GCC drops the other two stores.
void ActorsShared80131f9cSub0(GpEnemy* enemy, Task* task)
{
    Actor110300Work* work;
    GsCOORDINATE2*   coord;
    TmdObject*       obj;
    VECTOR           vec;

    obj                      = task->extra;
    coord                    = obj->field_8;
    work                     = (Actor110300Work*)Mem_Calloc(0x55C, 0);
    ActorsShared80131f9cWork = work;
    task->idMap              = (TaskIdMap*)work;
    if (work == 0) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }
    task->exitCallback      = func_actor_110300_80132088;
    coord->sub              = &Gfx_ViewCoord;
    enemy->field_4          = &coord->coord;
    enemy->node.field_4     = 1;
    enemy->field_48         = 0;
    enemy->node.field_5     = 0;
    obj->field_E            = 0;
    coord->flg              = 0;
    D_actor_110300_8013A0A4 = (GpActorWork*)task;
    D_actor_110300_8013A0A8 = Task_SpawnFromTable(&D_actor_110300_8013A06C, 1, 0, 0);
    func_800B3F84(&ActorsShared80131f9cWork->anim, D_actor_110300_8013A084, (GpAnimObj*)obj,
                  ActorsShared80131f9cWork->pad_334, ActorsShared80131f9cWork->slots);
    ActorsShared80131f9cWork->animId    = 1;
    ActorsShared80131f9cWork->field_474 = 2;
    func_actor_110300_801320C4((GpActorWork*)task);
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1];
    vec.vz = coord->workm.t[2];
    func_800D7A9C(obj, &vec, 0, 3);
    ActorsShared80131f9cWork->field_47A = ActorsShared80131f9cWork->field_47A + 1;
    task->field_24                      = D_actor_110300_8013A054;
    task->state                        += 1;
}

INCLUDE_RODATA("actors/nonmatchings/actor_110300/actor_110300", D_actor_110300_80131E20);
