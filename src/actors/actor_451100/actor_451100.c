#include "common.h"

#include "actors/actor_451100.h"
#include "actors/actor_461800_move.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/D4.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"

extern u8         D_actor_451100_8013F740[];
extern GpMsgEntry D_actor_451100_8013F704[];

/// State 0 of the `func_actor_451100_801322D4` dispatcher: allocates the work
/// block, publishes it in `D_actor_451100_8014E744` and on the task's work
/// slot, points the model's light and color matrices and its animation context
/// at it, then runs the per-frame update once and advances the task to state 1.
///
/// Every access to the block after the null check goes through
/// `D_actor_451100_8014E744` rather than the `memCalloc` result, which is why
/// the pointer is reloaded at each use.
void func_actor_451100_80131E24(GpEnemy* enemy, Task* task)
{
    VECTOR           vec;
    Actor451100Work* work;
    TmdObject*       obj;
    GsCOORDINATE2*   coord;

    obj                     = task->extra;
    coord                   = obj->coords;
    work                    = memCalloc(0x4B8, 0);
    D_actor_451100_8014E744 = work;
    task->work              = work;
    if (work == NULL) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }
    task->exitCallback      = func_actor_451100_801323B4;
    coord->sub              = &gGfxViewCoord;
    enemy->field_4          = &coord->coord;
    enemy->field_48         = 0;
    enemy->node.targeted    = 0;
    enemy->node.flags       = 1;
    obj->otOffset           = 1;
    obj->lightMtx           = &D_actor_451100_8014E744->light;
    obj->colorMtx           = &D_actor_451100_8014E744->color;
    vec.vx                  = coord->workm.t[0];
    vec.vy                  = coord->workm.t[1] - 0x320;
    D_actor_451100_8014E748 = task;
    vec.vz                  = coord->workm.t[2];
    func_800D7A9C(obj, &vec, 0, 3);
    func_800B3F84(&D_actor_451100_8014E744->anim, D_actor_451100_8013F740, obj,
                  &D_actor_451100_8014E744->slots[0x13], D_actor_451100_8014E744->slots);
    D_actor_451100_8014E744->animId  = 0x14;
    D_actor_451100_8014E744->state   = 2;
    D_actor_451100_8014E744->travel  = 0;
    D_actor_451100_8014E744->animArg = 0;
    task->msgTable                   = D_actor_451100_8013F704;
    func_actor_451100_80131F84(task);
    task->state += 1;
}

void func_actor_451100_80131F84(Task* task)
{
    GsCOORDINATE2*   coord = ((TmdObject*)task->extra)->coords;
    Actor451100Work* work  = (Actor451100Work*)task->work;

    if (D_actor_451100_8014E744->state == 1) {
        func_actor_451100_801324B8();
        D_actor_451100_8014E744->state = 3;
    } else if (D_actor_451100_8014E744->state == 2) {
        func_actor_451100_80132428();
        D_actor_451100_8014E744->state = 3;
    } else if (D_actor_451100_8014E744->state == 3) {
        if ((s16)work->animId == 0xE || (s16)work->animId == 2 || (s16)work->animId == 0xF) {
            if (work->travel != 0) {
                switch (D_actor_451100_8014E74C) {
                    case 0:
                        Actor461800_MoveForward(task, 0x3C);
                        break;
                    case 1:
                        Actor461800_MoveForward(task, -0xF);
                        break;
                    case 2:
                        Actor461800_MoveForward(task, 0x19);
                        break;
                }
                if (--work->travel == 0) {
                    work->state             = 1;
                    D_actor_451100_8013F700 = 10;
                    work->animId            = 0xD;
                }
            }
        }
        if ((s16)work->animId == 3 && (s16)work->animArg != 0) {
            work->yaw += 0x33;
            Gfx_RotMatrixY(&coord->coord, work->yaw, 1);
            coord->flg = 0;
            work->animArg--;
        }
        func_actor_451100_801323DC();
    }
}
