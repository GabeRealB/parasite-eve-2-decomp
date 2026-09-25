#include "common.h"

#include "actors/actor_260500.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/D4.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"

extern u8         D_actor_260500_80159DBC[];
extern GpMsgEntry D_actor_260500_80159D80[];

/// Spawn routine (state 0 of `func_actor_260500_8014A460`): allocates the work
/// block and publishes it in `D_actor_260500_80159E4C` and the task's `work`
/// slot, binds the model to the view and hands it the block's light and colour
/// matrices, publishes the task in `D_actor_260500_80159E50`, relights the
/// model from a point 0x320 above its translation and binds the animation
/// stream. It then installs the message table and runs the first update with
/// the reset mode 2 / id 4 it seeds.
void func_actor_260500_80149FB0(GpEnemy* enemy, Task* task)
{
    VECTOR         vec;
    GsCOORDINATE2* coord;
    TmdObject*     obj;
    void*          work;

    obj                     = task->extra;
    coord                   = obj->coords;
    work                    = memCalloc(sizeof(Actor260500Work), 0);
    D_actor_260500_80159E4C = work;
    task->work              = work;
    if (work == NULL) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }
    task->exitCallback      = func_actor_260500_8014A540;
    coord->sub              = &gGfxViewCoord;
    enemy->field_4          = &coord->coord;
    enemy->field_48         = 0;
    enemy->node.targeted    = 0;
    enemy->node.flags       = 1;
    obj->otOffset           = 1;
    obj->lightMtx           = &D_actor_260500_80159E4C->light;
    obj->colorMtx           = &D_actor_260500_80159E4C->color;
    vec.vx                  = coord->workm.t[0];
    vec.vy                  = coord->workm.t[1] - 0x320;
    D_actor_260500_80159E50 = task;
    vec.vz                  = coord->workm.t[2];
    func_800D7A9C(obj, &vec, 0, 3);
    func_800B3F84(&D_actor_260500_80159E4C->anim, D_actor_260500_80159DBC, obj,
                  D_actor_260500_80159E4C->poses, D_actor_260500_80159E4C->slots);
    D_actor_260500_80159E4C->field_480 = 4;
    D_actor_260500_80159E4C->field_47C = 2;
    D_actor_260500_80159E4C->field_4B2 = 0;
    D_actor_260500_80159E4C->field_4B4 = 0;
    task->msgTable                     = D_actor_260500_80159D80;
    func_actor_260500_8014A110(task);
    task->state++;
}

INCLUDE_ASM("actors/nonmatchings/actor_260500/actor_260500_3", func_actor_260500_8014A110);
