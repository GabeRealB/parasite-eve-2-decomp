#include "common.h"

#include "actors/actor_461800_move.h"
#include "actors/actor_535700.h"
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

void func_actor_535700_80131E24(Task* task)
{
    TILE* tile;

    if (D_actor_535700_80146840 != 0) {
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

/// State 0 of the first enemy's task: allocates the work block, publishes it
/// in `D_actor_535700_80146844` and on the task's work slot, points the
/// model's light and colour matrices and its animation context at it,
/// publishes the task in `D_actor_535700_80146848`, then runs the runner once
/// and advances the task to state 1.
///
/// Every access to the block after the null check goes through the global
/// rather than the `memCalloc` result, which is why the pointer is reloaded at
/// each use.
void func_actor_535700_80131FA0(GpEnemy* enemy, Task* task)
{
    VECTOR           vec;
    Actor535700Work* work;
    TmdObject*       obj;
    GsCOORDINATE2*   coord;

    obj                     = task->extra;
    coord                   = obj->coords;
    work                    = memCalloc(0x4C0, 0);
    D_actor_535700_80146844 = work;
    task->work              = work;
    if (work == NULL) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }
    task->exitCallback      = func_actor_535700_80132558;
    coord->sub              = &gGfxViewCoord;
    enemy->field_4          = &coord->coord;
    enemy->field_48         = 0;
    enemy->node.targeted    = 0;
    enemy->node.flags       = 1;
    obj->otOffset           = 1;
    obj->lightMtx           = &D_actor_535700_80146844->light;
    obj->colorMtx           = &D_actor_535700_80146844->color;
    vec.vx                  = coord->workm.t[0];
    vec.vy                  = coord->workm.t[1] - 0x320;
    D_actor_535700_80146848 = task;
    vec.vz                  = coord->workm.t[2];
    func_800D7A9C(obj, &vec, 0, 3);
    func_800B3F84(&D_actor_535700_80146844->anim, D_actor_535700_8013DAE8, obj,
                  &D_actor_535700_80146844->field_34C, D_actor_535700_80146844->slots);
    D_actor_535700_80146844->animId    = 1;
    D_actor_535700_80146844->state     = 2;
    D_actor_535700_80146844->field_4B2 = 0;
    D_actor_535700_80146844->field_4B4 = 0;
    D_actor_535700_80146844->field_4B8 = 0;
    D_actor_535700_80146844->field_4BC = 0;
    task->msgTable                     = D_actor_535700_8013DAAC;
    func_actor_535700_80132108(task);
    task->state += 1;
}

/// Per-frame update: states 1 and 2 run their one-shot animation reseed and
/// leave the work block in state 3; state 3 walks the model while `field_4B2`
/// counts down (distance picked by `D_actor_535700_8014684C`), turns it while
/// `field_4B4` counts down in animation 3, then ticks the animation and, once
/// `field_4BC` is set, plays the footsteps.
void func_actor_535700_80132108(Task* task)
{
    GsCOORDINATE2*   coord = ((TmdObject*)task->extra)->coords;
    Actor535700Work* work  = (Actor535700Work*)task->work;

    if (D_actor_535700_80146844->state == 1) {
        func_actor_535700_80132730();
        D_actor_535700_80146844->state = 3;
    } else if (D_actor_535700_80146844->state == 2) {
        func_actor_535700_80132694();
        D_actor_535700_80146844->state = 3;
    } else if (D_actor_535700_80146844->state == 3) {
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
            func_actor_535700_80132580(task);
        }
    }
}
