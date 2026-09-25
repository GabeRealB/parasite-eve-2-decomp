#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/inline_c.h>
#include "gte.h"

#include "actors/actor_151000.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
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
extern u8         D_80072729;

/// Steps the task's model `amount` units along its facing (the coordinate
/// matrix's z column, normalised and scaled on the GTE), using a scratch-pad
/// vector; skipped while `D_80072729` is 1.
static __inline__ void Actor151000_MoveForward(Task* task, s16 amount)
{
    GsCOORDINATE2* coord;
    SVECTOR*       head;
    SVECTOR*       vec;

    coord = ((TmdObject*)task->extra)->coords;
    if (D_80072729 != 1) {
        head                       = *(SVECTOR**)G_SCRATCH_HEAD;
        vec                        = head - 1;
        *(SVECTOR**)G_SCRATCH_HEAD = vec;
        Gfx_MatrixCol2(&coord->coord, vec);
        VectorNormalSS(vec, vec);
        gte_lddp(amount);
        gte_ldsv(vec);
        gte_gpf12();
        gte_stsv(vec);
        coord->coord.t[0]          += head[-1].vx;
        coord->coord.t[1]          += vec->vy;
        coord->coord.t[2]          += vec->vz;
        coord->flg                  = 0;
        *(SVECTOR**)G_SCRATCH_HEAD += 1;
    }
}

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
    D_actor_151000_8013D37C->animId     = 1;
    D_actor_151000_8013D37C->state      = 2;
    D_actor_151000_8013D37C->travel     = 0;
    D_actor_151000_8013D37C->turnFrames = 0;
    D_actor_151000_8013D37C->stepRec    = 0;
    D_actor_151000_8013D37C->footsteps  = 0;
    task->msgTable                      = D_actor_151000_8013D2B0;
    func_actor_151000_80132084(task);
    task->state += 1;
}

/// Per-frame update: states 1 and 2 run their one-shot animation restart and
/// leave the work block in state 3; state 3 walks the model while `travel`
/// counts down (distance picked by `D_actor_151000_8013D384`) and, when the
/// walk ends, queues clip 0xD through state 1; it turns the model while
/// `turnFrames` counts down in clip 3, then ticks the animation and, once
/// `footsteps` is set, plays the footsteps.
void func_actor_151000_80132084(Task* task)
{
    GsCOORDINATE2*   coord = ((TmdObject*)task->extra)->coords;
    Actor151000Work* work  = (Actor151000Work*)task->work;

    if (D_actor_151000_8013D37C->state == 1) {
        func_actor_151000_801326AC();
        D_actor_151000_8013D37C->state = 3;
    } else if (D_actor_151000_8013D37C->state == 2) {
        func_actor_151000_80132610();
        D_actor_151000_8013D37C->state = 3;
    } else if (D_actor_151000_8013D37C->state == 3) {
        if (work->animId == 0xE || work->animId == 2 || work->animId == 0xF) {
            if (work->travel != 0) {
                switch (D_actor_151000_8013D384) {
                    case 0:
                        Actor151000_MoveForward(task, 0x3C);
                        break;
                    case 1:
                        Actor151000_MoveForward(task, -0xF);
                        break;
                    case 2:
                        Actor151000_MoveForward(task, 0x19);
                        break;
                }
                if (--work->travel == 0) {
                    work->state             = 1;
                    D_actor_151000_8013D2AC = 10;
                    work->animId            = 0xD;
                }
            }
        }
        if (work->animId == 3 && work->turnFrames != 0) {
            work->yaw += 0x33;
            Gfx_RotMatrixY(&coord->coord, work->yaw, 1);
            coord->flg = 0;
            work->turnFrames--;
        }
        func_actor_151000_801325C4();
        if (work->footsteps != 0) {
            func_actor_151000_801324FC(task);
        }
    }
}
