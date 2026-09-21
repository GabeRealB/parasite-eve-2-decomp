#include "common.h"

#include "actors/actor_451100.h"
#include "actors/actor_461800_move.h"
#include "main/gfx.h"
#include "main/task.h"
#include "main/tmd.h"

INCLUDE_ASM("actors/nonmatchings/actor_451100/actor_451100", ActorsShared80131f9cSub0);

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

INCLUDE_RODATA("actors/nonmatchings/actor_451100/actor_451100", D_actor_451100_80131E20);
