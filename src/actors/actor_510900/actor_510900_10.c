#include "common.h"
#include "main/task.h"
#include "actors/actor_510900.h"

void func_actor_510900_8013A5B8(void* enemy, Task* task);
void func_actor_510900_8013A85C(void* enemy, Task* task);

void func_actor_510900_8013C1EC(Task* task)
{
    void (*fns[2])(void*, Task*) = { func_actor_510900_8013A5B8, func_actor_510900_8013A85C };

    fns[task->state](task->spawnArg2, task);
}

s32 Gp_GetViewIndex(void);

/// The three views the child is visible in, indexed by its `field_334`.
extern u16 D_actor_510900_80167CD8[][3];

/// Reports whether the camera has cut away from every view this child runs in.
/// Until then it returns 1 and the caller keeps ticking the animation; on the
/// frame all three views miss it hides the model, releases the task it holds
/// and returns 0.
s32 func_actor_510900_8013C240(Task* task)
{
    Actor510900Obj2C*     obj;
    Actor510900ChildAnim* work;
    Actor510900Ctx*       ctx;
    s32                   i;
    u8                    misses;
    u8                    view;

    obj    = (Actor510900Obj2C*)task->extra;
    work   = (Actor510900ChildAnim*)task->idMap;
    ctx    = (Actor510900Ctx*)task->spawnArg2;
    misses = 0;
    view   = Gp_GetViewIndex();
    for (i = 0; i < 3; i++) {
        if (view != D_actor_510900_80167CD8[work->field_334][i]) {
            misses++;
        }
    }

    if (misses != 3) {
        return 1;
    }

    obj->field_C        = 0x80;
    work->obj2BC.flags &= 0x7FFF;
    work->obj2F4.flags &= 0x7FFF;
    ctx->node.field_4   = 1;
    if (work->field_32C != NULL) {
        work->field_32C->state = 2;
        work->field_32C        = NULL;
    }
    if (work->field_332 != 0) {
        work->field_332--;
    }
    return 0;
}

void func_actor_510900_8013C338(Actor510900* arg0, Actor510900Coord* arg1)
{
    VECTOR pos;

    pos.vx = arg1->field_0.workm.t[0];
    pos.vy = arg1->field_0.workm.t[1];
    pos.vz = arg1->field_0.workm.t[2];
    func_800D7A9C((TmdObject*)arg0->field_2C, &pos, 0, 3);
}

void func_actor_510900_8013C380(Actor510900* arg0)
{
    GpEnemy*         enemy = arg0->field_20;
    Actor510900Work* work  = arg0->field_1C;

    Gp_UnlinkNode(&enemy->node);
    Gp_UnlinkObj(&work->obj2BC);
    Gp_UnlinkObj(&work->obj2F4);
    Gp_DestroyEnemy(enemy, (Task*)arg0);
}

void func_actor_510900_8013AD90(void* enemy, Task* task);
void func_actor_510900_8013AF38(void* enemy, Task* task);

void func_actor_510900_8013C3DC(Task* task)
{
    void (*fns[2])(void*, Task*) = { func_actor_510900_8013AD90, func_actor_510900_8013AF38 };

    fns[task->state](task->spawnArg2, task);
}

void func_actor_510900_8013C430(Actor510900* arg0)
{
    GpEnemy*         enemy = arg0->field_20;
    Actor510900Work* work  = arg0->field_1C;

    Gp_UnlinkNode(&enemy->node);
    Gp_UnlinkObj(&work->obj0);
    Gp_UnlinkObj(&work->obj38);
    Gp_DestroyEnemy(enemy, (Task*)arg0);
}
