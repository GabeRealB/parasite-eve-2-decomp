#include "common.h"

#include "actors/actor_451100.h"
#include "actors/actor_461800_move.h"
#include "actors/actors_shared_801326b4.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/D4.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"

extern u8         D_actor_451100_8013F740[];
extern GpMsgEntry D_actor_451100_8013F704[];

/// State 0 of the `ActorsShared80131f9c` dispatcher: allocate the work block,
/// publish it in `ActorsShared80131f9cWork` and on the task's work slot, point
/// the model's light and color matrices and its animation context at it, then
/// run the overlay's per-frame update once and advance the task to state 1.
///
/// Every access to the block after the null check goes through
/// `ActorsShared80131f9cWork` rather than the `memCalloc` result, which is why
/// the pointer is reloaded at each use.
void ActorsShared80131f9cSub0(GpEnemy* enemy, Task* task)
{
    VECTOR           vec;
    Actor451100Work* work;
    TmdObject*       obj;
    GsCOORDINATE2*   coord;

    obj                      = task->extra;
    coord                    = obj->coords;
    work                     = memCalloc(0x4B8, 0);
    ActorsShared80131f9cWork = work;
    task->work               = work;
    if (work == NULL) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }
    task->exitCallback       = func_actor_451100_801323B4;
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
    func_800B3F84(&ActorsShared80131f9cWork->anim, D_actor_451100_8013F740, obj,
                  &ActorsShared80131f9cWork->slots[0x13], ActorsShared80131f9cWork->slots);
    ActorsShared80131f9cWork->animId  = 0x14;
    ActorsShared80131f9cWork->state   = 2;
    ActorsShared80131f9cWork->travel  = 0;
    ActorsShared80131f9cWork->animArg = 0;
    task->msgTable                    = D_actor_451100_8013F704;
    func_actor_451100_80131F84(task);
    task->state += 1;
}

void ActorsShared80132428(void);

void func_actor_451100_80131F84(Task* task)
{
    GsCOORDINATE2*   coord = ((TmdObject*)task->extra)->coords;
    Actor451100Work* work  = (Actor451100Work*)task->work;

    if (ActorsShared80131f9cWork->state == 1) {
        func_actor_451100_801324B8();
        ActorsShared80131f9cWork->state = 3;
    } else if (ActorsShared80131f9cWork->state == 2) {
        ActorsShared80132428();
        ActorsShared80131f9cWork->state = 3;
    } else if (ActorsShared80131f9cWork->state == 3) {
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
