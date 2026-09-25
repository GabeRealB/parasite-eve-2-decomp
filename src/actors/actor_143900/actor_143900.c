#include "common.h"

#include "actors/actor_143900.h"
#include "actors/actor_461800_move.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/mc.h"
#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"

/// Spawn table this overlay hands to `Task_SpawnFromTable`. It sits at an
/// absolute address outside the actor slot - offset 0x440 into the loaded room
/// overlay, whose base is 0x8017D5C0 - so splat cannot name it and it keeps its
/// raw `D_` form.
extern TaskDesc D_8017DA00;

/// Arms `sceneEvent` and starts the room's spawn-table task, unless
/// `demoScene` is 9, so this story trigger is skipped while the attract demo
/// plays.
void func_actor_143900_80131E24(void)
{
    if (Mc_SaveData.demoScene != 9) {
        Mc_SaveData.sceneEvent = 0x14;
        Task_SpawnFromTable(&D_8017DA00, 0, 0, 0);
    }
}

/// Spawn routine of the first variant (state 0 of `func_actor_143900_80132324`):
/// allocates the work block and publishes it in `D_actor_143900_801496B8` and
/// the task's `work` slot, binds the model's coordinate to the view and hands
/// the object its light and colour matrices out of the block, publishes the
/// task in `D_actor_143900_801496BC`, relights the model from a point 0x320
/// above its translation, binds the animation stream and runs the first update
/// with the reset mode 2 / id 1 it seeds.
///
/// Every access to the block after the allocation goes through the global
/// rather than the `memCalloc` result, which is why the pointer is reloaded at
/// each use instead of staying in a callee-saved register.
void func_actor_143900_80131E70(GpEnemy* enemy, Task* task)
{
    VECTOR           vec;
    Actor143900Work* work;
    TmdObject*       obj;
    GsCOORDINATE2*   coord;

    obj                     = task->extra;
    coord                   = obj->coords;
    work                    = memCalloc(0x4F0, 0);
    D_actor_143900_801496B8 = work;
    task->work              = (TaskIdMap*)work;
    if (work == NULL) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }
    task->exitCallback      = func_actor_143900_80132404;
    coord->sub              = &gGfxViewCoord;
    enemy->field_4          = &coord->coord;
    enemy->field_48         = 0;
    enemy->node.targeted    = 0;
    enemy->node.flags       = 1;
    obj->otOffset           = 1;
    obj->flags              = 0;
    obj->lightMtx           = &D_actor_143900_801496B8->light;
    obj->colorMtx           = &D_actor_143900_801496B8->color;
    vec.vx                  = coord->workm.t[0];
    vec.vy                  = coord->workm.t[1] - 0x320;
    D_actor_143900_801496BC = task;
    vec.vz                  = coord->workm.t[2];
    func_800D7A9C(obj, &vec, 0, 3);
    func_800B3F84(&D_actor_143900_801496B8->anim, D_actor_143900_801413F8, obj,
                  &D_actor_143900_801496B8->pad_374, D_actor_143900_801496B8->slots);
    D_actor_143900_801496B8->field_4B8 = 1;
    D_actor_143900_801496B8->field_4B4 = 2;
    D_actor_143900_801496B8->field_4EA = 0;
    D_actor_143900_801496B8->field_4EC = 0;
    task->msgTable                     = D_actor_143900_801413BC;
    func_actor_143900_80131FD4(task);
    task->state += 1;
}

extern s16 D_actor_143900_801413B8;
extern s16 D_actor_143900_801496C0;

/// Per-frame update of the first variant: modes 1 and 2 run their one-shot
/// reseed and switch to mode 3; mode 3 walks the model while `field_4EA`
/// counts down (distance picked by `D_actor_143900_801496C0`), turns it while
/// `field_4EC` counts down in animation 3, then ticks the animation.
void func_actor_143900_80131FD4(Task* task)
{
    GsCOORDINATE2*   coord = ((TmdObject*)task->extra)->coords;
    Actor143900Work* work  = (Actor143900Work*)task->work;

    if (D_actor_143900_801496B8->field_4B4 == 1) {
        func_actor_143900_801325A4();
        D_actor_143900_801496B8->field_4B4 = 3;
    } else if (D_actor_143900_801496B8->field_4B4 == 2) {
        func_actor_143900_80132514();
        D_actor_143900_801496B8->field_4B4 = 3;
    } else if (D_actor_143900_801496B8->field_4B4 == 3) {
        if (work->field_4B8 == 0xE || work->field_4B8 == 2 || work->field_4B8 == 0xF) {
            if (work->field_4EA != 0) {
                switch (D_actor_143900_801496C0) {
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
                if (--work->field_4EA == 0) {
                    work->field_4B4         = 1;
                    D_actor_143900_801413B8 = 10;
                    work->field_4B8         = 0xD;
                }
            }
        }
        if (work->field_4B8 == 3 && work->field_4EC != 0) {
            work->yaw += 0x33;
            Gfx_RotMatrixY(&coord->coord, (s16)work->yaw, 1);
            coord->flg = 0;
            work->field_4EC--;
        }
        func_actor_143900_801324C8();
    }
}
