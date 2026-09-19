#include "common.h"

#include "actors/actor_461800_move.h"
#include "actors/actor_535700.h"
#include "actors/actors_shared_801324fc.h"
#include "gameplay/1BC.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/task.h"
#include "main/tmd.h"

extern s16 D_80071076;

void func_actor_535700_80131E2C(Task* task)
{
    TILE* tile;
    s32   count;

    if (count != 0) {
        tile           = (TILE*)Gpu_PrimCursor;
        Gpu_PrimCursor = (DR_TPAGE*)(tile + 1);
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
    if (Mc_SaveData.field_23 != 9) {
        Mc_SaveData.at4.loc.area = 0x1D;
        Mc_SaveData.at4.loc.warp = 5;
        Mc_SaveData.at4.loc.room = 2;
        D_80071076               = 1;
        Task_Spawn(0, 0x11, 0, 0);
        Mc_SaveData.field_5C5 = 6;
        Gp_RestoreStreamRng();
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_535700/actor_535700", ActorsShared80131f9cSub0);

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

INCLUDE_RODATA("actors/nonmatchings/actor_535700/actor_535700", D_actor_535700_80131E20);
