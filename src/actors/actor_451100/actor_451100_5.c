#include "common.h"

#include "actors/actor_451100.h"
#include "gameplay/1BC.h"
#include "gameplay/268.h"
#include "gameplay/3A34.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"

extern TaskDesc D_actor_451100_8014E6E4[];
extern u8       D_actor_451100_8014E6B4[];
extern u8       D_actor_451100_8014E6FC[];

INCLUDE_ASM("actors/nonmatchings/actor_451100/actor_451100_5", func_actor_451100_801326B0);

INCLUDE_ASM("actors/nonmatchings/actor_451100/actor_451100_5", func_actor_451100_8013280C);

/// State-0 spawn handler: allocates the actor's `Actor451100Work` block, hangs
/// it off the task, spawns the paired enemy and reparents this task under it,
/// then seeds the animation.
///
/// `memCalloc`'s result goes through an untyped `block` that `work` is copied
/// from: the raw pointer is what the `Task::work` store and the null test read,
/// so it stays a short-lived `$v0` quantity while the typed copy takes the
/// callee-saved home it needs across the calls below. Assigning the call result
/// straight to `work` - the shape the twin in `actor_161500` uses - collapses
/// the two into one pseudo and puts `$s1` in all three places.
void ActorsShared80131e24Sub0(GpEnemy* enemy, Task* task)
{
    VECTOR           vec;
    Actor451100Work* work;
    GsCOORDINATE2*   coord;
    TmdObject*       obj;
    GpEnemy*         spawned;
    void*            block;

    obj        = task->extra;
    coord      = obj->coords;
    block      = memCalloc(0x4C0, false);
    work       = (Actor451100Work*)block;
    task->work = (TaskIdMap*)block;
    if (block == NULL) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }
    task->exitCallback   = func_actor_451100_80132CAC;
    coord->sub           = &gGfxViewCoord;
    enemy->field_4       = &coord->coord;
    enemy->field_48      = 0;
    enemy->node.targeted = 0;
    enemy->node.flags    = 1;
    obj->otOffset        = 1;
    work->enemy          = enemy;
    spawned              = Gp_SpawnEnemyFromTable(D_actor_451100_8014E6E4, 1, 0, enemy);
    Task_Reparent(task, spawned->task);
    work->pairTask = spawned->task;
    obj->lightMtx  = &work->light;
    obj->colorMtx  = &work->color;
    vec.vx         = coord->workm.t[0];
    vec.vy         = coord->workm.t[1] - 0x320;
    vec.vz         = coord->workm.t[2];
    func_800D7A9C(obj, &vec, 0, 3);
    func_800B3F84(&work->anim, D_actor_451100_8014E6FC, (GpAnimObj*)obj, &work->slots[0x13],
                  work->slots);
    work->animId   = 1;
    work->state    = 2;
    task->msgTable = D_actor_451100_8014E6B4;
    ActorsShared80132a1c(task);
    task->state += 1;
}
