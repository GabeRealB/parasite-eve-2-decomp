#include "common.h"

#include "actors/actor_151000.h"

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

/// The fade task: while the countdown `D_actor_151000_8013D378` is non-zero,
/// draws a full-screen black `TILE` into ordering table slot 0xA; once it is
/// zero the task kills itself.
void func_actor_151000_80131E24(Task* task)
{
    TILE* tile;

    if (D_actor_151000_8013D378 != 0) {
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

/// Starts a fade to black lasting `frames` frames: seeds the countdown and,
/// unless it is zero, spawns the fade task.
void func_actor_151000_80131EE0(s32 frames)
{
    D_actor_151000_8013D378 = frames;
    if (frames != 0) {
        Task_SpawnFromTable(&D_actor_151000_80133360, 0, 0, 0);
    }
}

/// State 0 of the enemy's task: allocates the work block, publishes it in
/// `D_actor_151000_8013D37C` and on the task's work slot, points the model's
/// light and colour matrices and its animation context at it, publishes the
/// task in `D_actor_151000_8013D380`, then runs the runner once and advances
/// the task to state 1.
///
/// Every access to the block after the null check goes through the global
/// rather than the `memCalloc` result, which is why the pointer is reloaded at
/// each use.
void func_actor_151000_80131F1C(GpEnemy* enemy, Task* task)
{
    VECTOR           vec;
    Actor151000Work* work;
    TmdObject*       obj;
    GsCOORDINATE2*   coord;

    obj                     = task->extra;
    coord                   = obj->coords;
    work                    = memCalloc(0x4C0, 0);
    D_actor_151000_8013D37C = work;
    task->work              = work;
    if (work == NULL) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }
    task->exitCallback      = func_actor_151000_801324D4;
    coord->sub              = &gGfxViewCoord;
    enemy->field_4          = &coord->coord;
    enemy->field_48         = 0;
    enemy->node.targeted    = 0;
    enemy->node.flags       = 1;
    obj->otOffset           = 1;
    obj->lightMtx           = &D_actor_151000_8013D37C->light;
    obj->colorMtx           = &D_actor_151000_8013D37C->color;
    vec.vx                  = coord->workm.t[0];
    vec.vy                  = coord->workm.t[1] - 0x320;
    D_actor_151000_8013D380 = task;
    vec.vz                  = coord->workm.t[2];
    func_800D7A9C(obj, &vec, 0, 3);
    func_800B3F84(&D_actor_151000_8013D37C->anim, D_actor_151000_8013D2EC, obj,
                  &D_actor_151000_8013D37C->field_34C, D_actor_151000_8013D37C->slots);
    D_actor_151000_8013D37C->field_480 = 1;
    D_actor_151000_8013D37C->field_47C = 2;
    D_actor_151000_8013D37C->field_4B2 = 0;
    D_actor_151000_8013D37C->field_4B4 = 0;
    D_actor_151000_8013D37C->field_4B8 = 0;
    D_actor_151000_8013D37C->field_4BC = 0;
    task->msgTable                     = D_actor_151000_8013D2B0;
    func_actor_151000_80132084(task);
    task->state += 1;
}

INCLUDE_ASM("actors/nonmatchings/actor_151000/actor_151000", func_actor_151000_80132084);
