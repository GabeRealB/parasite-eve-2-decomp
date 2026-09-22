#include "common.h"

#include "actors/actor_260500.h"
#include "actors/actors_shared_801326b4.h"
#include "actors/actors_shared_801366fc.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/D4.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"

extern u8         D_actor_260500_80159DBC[];
extern GpMsgEntry D_actor_260500_80159D80[];

/// Step 0 of the `ActorsShared80131f9c` dispatcher: allocate and publish the
/// work block, point the model at its light and colour matrices and seed its
/// animation context, then install the overlay's message table and run the
/// first step body.
void ActorsShared80131f9cSub0(GpEnemy* enemy, Task* task)
{
    VECTOR         vec;
    GsCOORDINATE2* coord;
    TmdObject*     obj;
    void*          work;

    obj                      = task->extra;
    coord                    = obj->coords;
    work                     = memCalloc(0x4B8, 0);
    ActorsShared80131f9cWork = work;
    task->work               = work;
    if (work == NULL) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }
    task->exitCallback       = ActorsShared801366fc;
    coord->sub               = &gGfxViewCoord;
    enemy->field_4           = &coord->coord;
    enemy->field_48          = 0;
    enemy->node.targeted     = 0;
    enemy->node.flags        = 1;
    obj->otOffset            = 1;
    obj->lightMtx            = &ActorsShared80131f9cWork->light;
    obj->colorMtx            = &ActorsShared80131f9cWork->color;
    vec.vx                   = coord->workm.t[0];
    vec.vy                   = coord->workm.t[1] - 0x320;
    ActorsShared801326b4Task = task;
    vec.vz                   = coord->workm.t[2];
    func_800D7A9C(obj, &vec, 0, 3);
    func_800B3F84(&ActorsShared80131f9cWork->anim, D_actor_260500_80159DBC, obj,
                  ActorsShared80131f9cWork->poses, ActorsShared80131f9cWork->slots);
    ActorsShared80131f9cWork->field_480 = 4;
    ActorsShared80131f9cWork->field_47C = 2;
    ActorsShared80131f9cWork->field_4B2 = 0;
    ActorsShared80131f9cWork->field_4B4 = 0;
    task->msgTable                      = D_actor_260500_80159D80;
    func_actor_260500_8014A110(task);
    task->state++;
}

INCLUDE_ASM("actors/nonmatchings/actor_260500/actor_260500_3", func_actor_260500_8014A110);
