#include "common.h"

#include "actors/actor_110800.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"

/// Step 0 of the `ActorsShared80131f9c` dispatcher: allocate the work block,
/// publish it, and hand the model's animation context its slot array.
///
/// Every access to the block goes through `ActorsShared80131f9cWork` rather
/// than the `Mem_Calloc` result, which is why the pointer is reloaded at each
/// use instead of staying in a callee-saved register. `task->field_24` takes
/// the message table the step-1 handler leaves behind.
///
/// Instruction-for-instruction the `actor_110300` copy of this body; only the
/// data symbols it relocates against are this overlay's own.
void ActorsShared80131f9cSub0(GpEnemy* enemy, Task* task)
{
    VECTOR         vec;
    void*          work;
    TmdObject*     obj;
    GsCOORDINATE2* coord;

    obj                      = task->extra;
    coord                    = obj->field_8;
    work                     = Mem_Calloc(0x55C, 0);
    ActorsShared80131f9cWork = work;
    task->idMap              = work;
    if (work == NULL) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }
    task->exitCallback      = func_actor_110800_8013232C;
    coord->sub              = &Gfx_ViewCoord;
    enemy->field_4          = &coord->coord;
    enemy->node.field_4     = 1;
    enemy->field_48         = 0;
    enemy->node.field_5     = 0;
    obj->field_E            = 0;
    coord->flg              = 0;
    D_actor_110800_80139F14 = (GpActorWork*)task;
    D_actor_110800_80139F18 = Task_SpawnFromTable(D_actor_110800_80139EDC, 1, 0, 0);
    func_800B3F84(&ActorsShared80131f9cWork->anim, D_actor_110800_80139EF4, (GpAnimObj*)obj,
                  ActorsShared80131f9cWork->aux, ActorsShared80131f9cWork->slots);
    ActorsShared80131f9cWork->animId    = 1;
    ActorsShared80131f9cWork->field_474 = 2;
    func_actor_110800_80132368(task);
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1];
    vec.vz = coord->workm.t[2];
    func_800D7A9C(obj, &vec, 0, 3);
    ActorsShared80131f9cWork->field_47A++;
    task->field_24 = D_actor_110800_80139EC4;
    task->state++;
}

INCLUDE_ASM("actors/nonmatchings/actor_110800/actor_110800", ActorsShared80131f9cSub1);

INCLUDE_RODATA("actors/nonmatchings/actor_110800/actor_110800", D_actor_110800_80131E20);
