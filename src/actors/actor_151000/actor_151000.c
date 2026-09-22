#include "common.h"

INCLUDE_RODATA("actors/nonmatchings/actor_151000/actor_151000", D_actor_151000_80131E24);

#include "actors/actor_151000.h"
#include "actors/actors_shared_801326b4.h"
#include "actors/actors_shared_801366fc.h"

#include "gameplay/1BC.h"
#include "gameplay/D4.h"
#include "main/display.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"

extern TaskDesc   D_actor_151000_80133360;
extern s32        D_actor_151000_8013D378;
extern u8         D_actor_151000_8013D2EC[];
extern GpMsgEntry D_actor_151000_8013D2B0[];

void func_actor_151000_80131E2C(Task* task)
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
}

void func_actor_151000_80131EE0(s32 arg0)
{
    D_actor_151000_8013D378 = arg0;
    if (arg0 != 0) {
        Task_SpawnFromTable(&D_actor_151000_80133360, 0, 0, 0);
    }
}

/// State 0 of the `ActorsShared80131f9c` dispatcher: allocate the work block,
/// publish it in `ActorsShared80131f9cWork` and on the task's work slot, point
/// the model's light and color matrices and its animation context at it.
///
/// Every access to the block goes through `ActorsShared80131f9cWork` rather
/// than the `memCalloc` result, which is why the pointer is reloaded at each
/// use instead of staying in a callee-saved register; the same two loads
/// publish the block's matrices. `task->msgTable` takes
/// `D_actor_151000_8013D2B0`.
///
/// The position it forwards to `func_800D7A9C` is the model root's translation
/// with its Y dropped by 0x320 — the standing height the light solve is cast
/// from, the ground offset every carrier of this body applies.
void ActorsShared80131f9cSub0(GpEnemy* enemy, Task* task)
{
    VECTOR           vec;
    Actor151000Work* work;
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
    func_800B3F84(&ActorsShared80131f9cWork->anim, D_actor_151000_8013D2EC, obj,
                  &ActorsShared80131f9cWork->field_34C, ActorsShared80131f9cWork->slots);
    ActorsShared80131f9cWork->field_480  = 1;
    ActorsShared80131f9cWork->field_47C  = 2;
    ActorsShared80131f9cWork->field_4B2  = 0;
    ActorsShared80131f9cWork->field_4B4  = 0;
    ActorsShared80131f9cWork->field_4B8  = 0;
    ActorsShared80131f9cWork->pad_4BC[0] = 0;
    task->msgTable                       = D_actor_151000_8013D2B0;
    func_actor_151000_80132084(task);
    task->state += 1;
}

INCLUDE_ASM("actors/nonmatchings/actor_151000/actor_151000", func_actor_151000_80132084);
