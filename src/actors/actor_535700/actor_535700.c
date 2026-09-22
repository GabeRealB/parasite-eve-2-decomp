#include "common.h"

INCLUDE_RODATA("actors/nonmatchings/actor_535700/actor_535700", D_actor_535700_80131E24);

#include "actors/actor_461800_move.h"
#include "actors/actor_535700.h"
#include "actors/actors_shared_801324fc.h"
#include "actors/actors_shared_801326b4.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/D4.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"

extern s16        D_80071076;
extern u8         D_actor_535700_8013DAE8[];
extern GpMsgEntry D_actor_535700_8013DAAC[];

void func_actor_535700_80131E2C(Task* task)
{
    TILE* tile;
    s32   count;

    if (count != 0) {
        tile           = (TILE*)gGpuPrimCursor;
        gGpuPrimCursor = tile + 1;
        SetTile(tile);
        tile->r0 = 0;
        tile->g0 = 0;
        tile->b0 = 0;
        tile->x0 = -0xA0;
        tile->y0 = -0x80;
        tile->w  = 0x140;
        tile->h  = 0x100;
        addPrim(gGpuCurrentOt + 0xA, tile);
    } else {
        taskKill(task);
    }
    D_actor_535700_80146840--;
}

INCLUDE_ASM("actors/nonmatchings/actor_535700/actor_535700", func_actor_535700_80131EF0);

void func_actor_535700_80131F2C(void)
{
    if (Mc_SaveData.demoScene != 9) {
        Mc_SaveData.at4.loc.area = 0x1D;
        Mc_SaveData.at4.loc.warp = 5;
        Mc_SaveData.at4.loc.room = 2;
        D_80071076               = 1;
        Task_Spawn(0, 0x11, 0, 0);
        Mc_SaveData.sceneEvent = 6;
        Gp_RestoreStreamRng();
    }
}

/// State 0 of the `ActorsShared80131f9c` dispatcher: allocate the work block,
/// publish it in `ActorsShared80131f9cWork` and on the task's work slot, point
/// the model's light and color matrices and its animation context at it, then
/// run the overlay's runner once and advance the task to state 1.
///
/// Every access to the block after the null check goes through
/// `ActorsShared80131f9cWork` rather than the `memCalloc` result, which is why
/// the pointer is reloaded at each use.
void ActorsShared80131f9cSub0(GpEnemy* enemy, Task* task)
{
    VECTOR           vec;
    Actor535700Work* work;
    TmdObject*       obj;
    GsCOORDINATE2*   coord;

    obj                      = task->extra;
    coord                    = obj->coords;
    work                     = memCalloc(0x4C0, 0);
    ActorsShared80131f9cWork = work;
    task->work               = work;
    if (work == NULL) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }
    task->exitCallback       = func_actor_535700_80132558;
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
    func_800B3F84(&ActorsShared80131f9cWork->anim, D_actor_535700_8013DAE8, obj,
                  &ActorsShared80131f9cWork->field_34C, ActorsShared80131f9cWork->slots);
    ActorsShared80131f9cWork->animId    = 1;
    ActorsShared80131f9cWork->state     = 2;
    ActorsShared80131f9cWork->field_4B2 = 0;
    ActorsShared80131f9cWork->field_4B4 = 0;
    ActorsShared80131f9cWork->field_4B8 = 0;
    ActorsShared80131f9cWork->field_4BC = 0;
    task->msgTable                      = D_actor_535700_8013DAAC;
    func_actor_535700_80132108(task);
    task->state += 1;
}

void ActorsShared80132610(void);

/// Per-frame update: states 1 and 2 run their one-shot animation reseed and
/// leave the work block in state 3; state 3 walks the model while `field_4B2`
/// counts down (distance picked by `D_actor_535700_8014684C`), turns it while
/// `field_4B4` counts down in animation 3, then ticks the animation. Same body
/// as `func_actor_461800_801331E4`.
void func_actor_535700_80132108(Task* task)
{
    GsCOORDINATE2*   coord = ((TmdObject*)task->extra)->coords;
    Actor535700Work* work  = (Actor535700Work*)task->work;

    if (ActorsShared80131f9cWork->state == 1) {
        func_actor_535700_80132730();
        ActorsShared80131f9cWork->state = 3;
    } else if (ActorsShared80131f9cWork->state == 2) {
        ActorsShared80132610();
        ActorsShared80131f9cWork->state = 3;
    } else if (ActorsShared80131f9cWork->state == 3) {
        if (work->animId == 0xE || work->animId == 2 || work->animId == 0xF) {
            if (work->field_4B2 != 0) {
                switch (D_actor_535700_8014684C) {
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
                if (--work->field_4B2 == 0) {
                    work->state             = 1;
                    D_actor_535700_8013DAA8 = 10;
                    work->animId            = 0xD;
                }
            }
        }
        if (work->animId == 3 && work->field_4B4 != 0) {
            work->field_4AE += 0x33;
            Gfx_RotMatrixY(&coord->coord, work->field_4AE, 1);
            coord->flg = 0;
            work->field_4B4--;
        }
        func_actor_535700_80132648();
        if (work->field_4BC != 0) {
            ActorsShared801324fc(task);
        }
    }
}
