#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/inline_c.h>
#include "gte.h"

#include "actors/actor_143900.h"
#include "actors/actor_461800_move.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/gameplay.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"

/// Approach mode the last `func_actor_143900_8013279C` call selected; the first
/// variant's update picks its walk distance from it.
extern s16 D_actor_143900_801496C0;

/// Approach mode the last `func_actor_143900_801333C4` call selected; the
/// second variant's update picks its walk distance from it.
extern s16 D_actor_143900_801496CC;

void func_actor_143900_80132F14(Task* task);
void func_actor_143900_80133068(void);
void func_actor_143900_801330B4(void);
void func_actor_143900_80133144(void);

/// Message 0x7DB handler of the first variant: when the payload's halfword at
/// 0x2 is zero, starts a 0x14-step turn, which the update performs while the
/// model plays animation 3.
s32 func_actor_143900_80132778(Task* task, s32 arg1, Actor143900Msg* msg)
{
    if (msg->field_2 == 0) {
        D_actor_143900_801496B8->field_4EC = 0x14;
    }
    return 0;
}

/// Message 0x7DD handler of the first variant: turns the model to face
/// `target` -- away from it in mode 1 -- and stores the per-step distance of the
/// walk the update then performs: the planar distance over 60 steps in mode 0,
/// 15 in mode 1 and 25 otherwise.
s32 func_actor_143900_8013279C(Task* task, s32 arg1, VECTOR* target, s32 mode)
{
    GsCOORDINATE2*   coord;
    Actor143900Work* work;
    s32              dx;
    s32              dz;
    s32              steps;
    s32              dist;
    s32              angle;

    coord                   = ((TmdObject*)task->extra)->coords;
    work                    = (Actor143900Work*)task->work;
    D_actor_143900_801496C0 = mode;
    dx                      = target->vx - coord->coord.t[0];
    dz                      = target->vz - coord->coord.t[2];
    angle                   = ratan2(dx, dz);
    work->yaw               = angle;
    if (D_actor_143900_801496C0 == 1) {
        work->yaw = angle + 0x800;
    }
    Gfx_RotMatrixY(&coord->coord, (s16)work->yaw, 1);
    dist  = SquareRoot0(dx * dx + dz * dz);
    steps = 0x19;
    switch (D_actor_143900_801496C0) {
        case 0:
            steps = 0x3C;
            break;
        case 1:
            steps = 0xF;
            break;
        case 2:
            break;
    }
    work->field_4EA = dist / steps;
    return 0;
}

/// Spawn routine of the second variant (state 0 of `func_actor_143900_80132DEC`):
/// allocates the 0x4F8 work block and publishes it in `D_actor_143900_801496C4`
/// and the task's `work` slot, binds the model's coordinate to the view and
/// hands the object its light and colour matrices out of the block, publishes
/// the task in `D_actor_143900_801496C8`, relights the model from a point 0x320
/// above its translation and binds the animation stream. It then starts the two
/// helper tasks from the overlay's spawn table and runs the first update with
/// the reset mode 2 / id 1 it seeds.
void func_actor_143900_801328D4(GpEnemy* enemy, Task* task)
{
    VECTOR            vec;
    Actor143900Work2* work;
    GsCOORDINATE2*    coord;
    TmdObject*        obj;
    Task*             helper;

    obj                     = task->extra;
    coord                   = obj->coords;
    work                    = memCalloc(0x4F8, false);
    D_actor_143900_801496C4 = work;
    task->work              = (TaskIdMap*)work;
    if (work == NULL) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }
    task->exitCallback      = func_actor_143900_80132ECC;
    coord->sub              = &gGfxViewCoord;
    enemy->field_4          = &coord->coord;
    enemy->node.flags       = 1;
    enemy->field_48         = 0;
    enemy->node.targeted    = 0;
    obj->otOffset           = 0x10;
    obj->lightMtx           = &D_actor_143900_801496C4->light;
    obj->colorMtx           = &D_actor_143900_801496C4->color;
    obj->flags              = 0;
    vec.vx                  = coord->workm.t[0];
    vec.vy                  = coord->workm.t[1] - 0x320;
    vec.vz                  = coord->workm.t[2];
    D_actor_143900_801496C8 = task;
    func_800D7A9C(obj, &vec, 0, 3);
    func_800B3F84(&D_actor_143900_801496C4->anim, D_actor_143900_80149688, obj,
                  &D_actor_143900_801496C4->pose, D_actor_143900_801496C4->slots);
    D_actor_143900_801496C4->field_4B8 = 1;
    D_actor_143900_801496C4->field_4B4 = 2;
    helper                             = Task_SpawnFromTable(D_actor_143900_80149664, 1, 1, 0);
    if (helper != NULL) {
        D_actor_143900_801496C4->field_4F0 = helper;
    }
    helper = Task_SpawnFromTable(D_actor_143900_80149664, 2, 0xC, 0);
    if (helper != NULL) {
        D_actor_143900_801496C4->field_4F4 = helper;
    }
    D_actor_143900_801496C4->field_4EA = 0;
    D_actor_143900_801496C4->field_4EC = 0;
    task->msgTable                     = D_actor_143900_80149634;
    func_actor_143900_80132A9C(task);
    task->state++;
}

/// Per-frame update of the second variant: modes 1 and 2 run their one-shot
/// reseed and switch to mode 3; mode 3 walks the model while `field_4EA`
/// counts down (distance picked by `D_actor_143900_801496CC`), turns it while
/// `field_4EC` counts down in animation 3, then ticks the animation.
void func_actor_143900_80132A9C(Task* task)
{
    GsCOORDINATE2*    coord = ((TmdObject*)task->extra)->coords;
    Actor143900Work2* work  = (Actor143900Work2*)task->work;

    if (D_actor_143900_801496C4->field_4B4 == 1) {
        func_actor_143900_80133144();
        D_actor_143900_801496C4->field_4B4 = 3;
    } else if (D_actor_143900_801496C4->field_4B4 == 2) {
        func_actor_143900_801330B4();
        D_actor_143900_801496C4->field_4B4 = 3;
    } else if (D_actor_143900_801496C4->field_4B4 == 3) {
        if (work->field_4B8 == 0xE || work->field_4B8 == 2 || work->field_4B8 == 0xF) {
            if (work->field_4EA != 0) {
                switch (D_actor_143900_801496CC) {
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
                    D_actor_143900_80149630 = 10;
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
        func_actor_143900_80133068();
    }
}

/// Two-state dispatcher: publishes the task's work block in
/// `D_actor_143900_801496C4` on the way through, then calls the handler its
/// state selects.
void func_actor_143900_80132DEC(Task* task)
{
    void (*fns[2])(GpEnemy*, Task*) = {
        func_actor_143900_801328D4,
        func_actor_143900_80132E48,
    };
    u8 scratch[0x40]; /* never referenced; only reserves the frame */

    D_actor_143900_801496C4 = (Actor143900Work2*)task->work;
    fns[task->state](task->spawnArg2, task);
}

/// Second state of the second variant's task: refreshes the model root's world
/// matrix, relights the model from a point 0x320 above its translation, then
/// runs the per-frame update and draws the ground shadow.
void func_actor_143900_80132E48(GpEnemy* enemy, Task* task)
{
    TmdObject*     obj;
    GsCOORDINATE2* coord;
    VECTOR         vec;

    obj   = task->extra;
    coord = obj->coords;
    Gp_UpdateCoord(coord);
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1] - 0x320;
    vec.vz = coord->workm.t[2];
    func_800D7A9C(obj, &vec, 0, 3);
    func_actor_143900_80132A9C(task);
    func_actor_143900_80132F14(task);
}
