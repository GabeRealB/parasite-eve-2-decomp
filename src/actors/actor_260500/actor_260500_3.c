#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/inline_c.h>
#include "gte.h"

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

extern u8 D_80072729;

/// Steps the task's model `amount` units along its facing (the coordinate
/// matrix's z column, normalised and scaled on the GTE), using a scratch-pad
/// vector; skipped while `D_80072729` is 1.
static __inline__ void Actor260500_MoveForward(Task* task, s16 amount)
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

/// Per-frame update: reset modes 1 and 2 run their one-shot reseed (blended or
/// plain) and switch to mode 3. In mode 3 the walking animations 2, 0xE and 0xF
/// step the model forward while `field_4B2` counts down, by a distance the
/// approach mode in `D_actor_260500_80159E54` picks, and blend into animation
/// 0xD with reset argument 10 when the walk ends; animation 3 turns the model
/// while `field_4B4` counts down. Mode 3 then ticks the animation.
void func_actor_260500_8014A110(Task* task)
{
    GsCOORDINATE2*   coord = ((TmdObject*)task->extra)->coords;
    Actor260500Work* work  = (Actor260500Work*)task->work;

    if (D_actor_260500_80159E4C->field_47C == 1) {
        func_actor_260500_8014A644();
        D_actor_260500_80159E4C->field_47C = 3;
    } else if (D_actor_260500_80159E4C->field_47C == 2) {
        func_actor_260500_8014A5B4();
        D_actor_260500_80159E4C->field_47C = 3;
    } else if (D_actor_260500_80159E4C->field_47C == 3) {
        if (work->field_480 == 0xE || work->field_480 == 2 || work->field_480 == 0xF) {
            if (work->field_4B2 != 0) {
                switch (D_actor_260500_80159E54) {
                    case 0:
                        Actor260500_MoveForward(task, 0x3C);
                        break;
                    case 1:
                        Actor260500_MoveForward(task, -0xF);
                        break;
                    case 2:
                        Actor260500_MoveForward(task, 0x19);
                        break;
                }
                if (--work->field_4B2 == 0) {
                    work->field_47C         = 1;
                    D_actor_260500_80159D7C = 10;
                    work->field_480         = 0xD;
                }
            }
        }
        if (work->field_480 == 3 && work->field_4B4 != 0) {
            work->yaw += 0x33;
            Gfx_RotMatrixY(&coord->coord, work->yaw, 1);
            coord->flg = 0;
            work->field_4B4--;
        }
        func_actor_260500_8014A568();
    }
}
