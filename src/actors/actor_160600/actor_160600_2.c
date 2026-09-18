#include "common.h"

#include "main/gfx.h"
#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"

#include "actors/actor_160600.h"
#include "actors/actors_shared_801366fc.h"
#include "actors/actors_shared_8014c874.h"

extern u8 D_actor_160600_8013DFAC[];
extern u8 D_actor_160600_8013DF70[];

void ActorsShared80131e24Sub0(GpEnemy* enemy, Task* task)
{
    VECTOR           vec;
    Actor160600Work* work;
    Actor160600Work* mem;
    GsCOORDINATE2*   coord;
    TmdObject*       obj;

    obj        = task->extra;
    coord      = obj->coords;
    mem        = (Actor160600Work*)Mem_Calloc(0x4F8, false);
    work       = mem;
    task->work = (TaskIdMap*)mem;
    if (mem == NULL) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }
    task->exitCallback  = ActorsShared801366fc;
    coord->sub          = &Gfx_ViewCoord;
    enemy->field_4      = &coord->coord;
    enemy->field_48     = 0;
    enemy->node.field_5 = 0;
    enemy->node.field_4 = 1;
    obj->otOffset       = 1;
    obj->flags          = 0;
    work->animId        = 10;
    work->enemy         = enemy;
    obj->lightMtx       = &work->light;
    obj->colorMtx       = &work->color;
    vec.vx              = coord->workm.t[0];
    vec.vy              = coord->workm.t[1] - 0x320;
    vec.vz              = coord->workm.t[2];
    func_800D7A9C(obj, &vec, 0, 3);
    func_800B3F84(&work->anim, D_actor_160600_8013DFAC, (GpAnimObj*)obj,
                  &work->field_374, work->slots);
    work->state    = 2;
    task->msgTable = D_actor_160600_8013DF70;
    ActorsShared8014c874(task);
    task->state++;
}
