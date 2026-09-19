#include "common.h"

#include "actors/actor_143900.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"

/// Seeds the task's `TmdObject` coordinate frame from `placement`: only the yaw
/// is used, remembered in the work block and applied with `Gfx_RotMatrixY`,
/// then the three longs become the coordinate's translation.
s32 func_actor_143900_801326FC(Task* task, s32 arg1, ActorShared8013411cPlacement* placement)
{
    GsCOORDINATE2* coord;
    u16            yaw;

    coord                         = ((TmdObject*)task->extra)->coords;
    ActorsShared80131f9cWork->yaw = yaw = placement->rot.vy;
    Gfx_RotMatrixY(&coord->coord, (s16)yaw, 1);
    coord->coord.t[0] = placement->pos.vx;
    coord->coord.t[1] = placement->pos.vy;
    coord->coord.t[2] = placement->pos.vz;
    coord->flg        = 0;
    return 0;
}

/// Message 0x7DB handler: while the payload's halfword at 0x2 is zero, latch
/// the work block's animation reset argument to 0x14.
s32 func_actor_143900_80132778(Task* task, s32 arg1, Actor143900Msg* msg)
{
    if (msg->field_2 == 0) {
        ActorsShared80131f9cWork->field_4EC = 0x14;
    }
    return 0;
}

INCLUDE_ASM("actors/nonmatchings/actor_143900/actor_143900_5", func_actor_143900_8013279C);

/// State-0 spawn routine of the actor's own variant: allocates the 0x4F8 work
/// block and publishes it in `D_actor_143900_801496C4` and the task's `work`
/// slot, binds the model's coordinate to the view (`sub`) and hands the object
/// its light and colour matrices out of the block, then points the object at
/// the part's world translation, dropped by 0x320 in y, for the colour matrix.
/// The two helper tasks come from the overlay's spawn table; the block's
/// animation state is reset to mode 1 / id 2 before the shared tick runs.
void func_actor_143900_801328D4(GpEnemy* enemy, Task* task)
{
    VECTOR                    vec;
    ActorsShared80132eccWork* work;
    GsCOORDINATE2*            coord;
    TmdObject*                obj;
    Task*                     helper;

    obj                     = task->extra;
    coord                   = obj->coords;
    work                    = memCalloc(0x4F8, false);
    D_actor_143900_801496C4 = work;
    task->work              = (TaskIdMap*)work;
    if (work == NULL) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }
    task->exitCallback      = ActorsShared80132ecc;
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

INCLUDE_ASM("actors/nonmatchings/actor_143900/actor_143900_5", func_actor_143900_80132A9C);

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

    D_actor_143900_801496C4 = (ActorsShared80132eccWork*)task->work;
    fns[task->state](task->spawnArg2, task);
}

INCLUDE_ASM("actors/nonmatchings/actor_143900/actor_143900_5", func_actor_143900_80132E48);
