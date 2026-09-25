#include "common.h"

#include "actors/actor_451100.h"
#include "gameplay/1BC.h"
#include "gameplay/268.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"

extern TaskDesc D_actor_451100_8014E6E4[];
extern u8       D_actor_451100_8014E6B4[];
extern u8       D_actor_451100_8014E6FC[];

/* Scratchpad stack pointer, initialised by GameMain (see src/main/gamemain.c). */
#define SCRATCH_SP (*(u32*)0x1F8003FC)

void Gp_DrawEffGroundQuad(VECTOR3* arg0, s32 arg1, s16 arg2);

/// Message 0x7DD handler of `D_actor_451100_8013F704`, the "walk to" opcode:
/// records `mode` in `D_actor_451100_8014E74C`, turns the model to face
/// `target` (away from it in mode 1) caching the yaw in the work block, and
/// leaves in `travel` the number of frames the step routine needs to cover the
/// planar distance at that mode's stride: 0x3C in mode 0, 0xF in mode 1 and
/// 0x19 in mode 2.
s32 func_actor_451100_801326B0(Task* task, s32 arg1, VECTOR* target, s32 mode)
{
    GsCOORDINATE2*   coord;
    Actor451100Work* work;
    s32              dx;
    s32              dz;
    s32              steps;
    s32              dist;
    s32              angle;

    coord                   = ((TmdObject*)task->extra)->coords;
    work                    = (Actor451100Work*)task->work;
    D_actor_451100_8014E74C = mode;
    dx                      = target->vx - coord->coord.t[0];
    dz                      = target->vz - coord->coord.t[2];
    angle                   = ratan2(dx, dz);
    work->yaw               = angle;
    if (D_actor_451100_8014E74C == 1) {
        work->yaw = angle + 0x800;
    }
    Gfx_RotMatrixY(&coord->coord, work->yaw, 1);
    dist = SquareRoot0(dx * dx + dz * dz);
    switch (D_actor_451100_8014E74C) {
        case 0:
            steps = 0x3C;
            break;
        case 1:
            steps = 0xF;
            break;
        case 2:
            steps = 0x19;
            break;
    }
    work->travel = dist / steps;
    return 0;
}

/// Draws the ground shadow quad under the model root of the actor
/// `func_actor_451100_801322D4` dispatches, unless the model is hidden
/// (`flags & 0x80`) or has no buffer yet. The root's world translation is
/// staged in a scratchpad `VECTOR3`, and the quad's brightness follows the
/// room's current ground shade.
void func_actor_451100_8013280C(Task* task)
{
    TmdObject*     obj;
    GsCOORDINATE2* coord;
    VECTOR3*       vec;

    obj   = (TmdObject*)task->extra;
    coord = obj->coords;
    if (!(obj->flags & 0x80) && obj->buffer != NULL) {
        vec     = (VECTOR3*)(SCRATCH_SP -= 0x18);
        vec->vx = coord->workm.t[0];
        vec->vy = coord->workm.t[1];
        vec->vz = coord->workm.t[2];
        Gp_DrawEffGroundQuad(vec, 0x200, Gp_State1C->groundShade);
        SCRATCH_SP += 0x18;
    }
}

/// State 0 of the `func_actor_451100_80132BD4` dispatcher: allocates the
/// actor's 0x4C0-byte `Actor451100Work` block and hangs it off the task, spawns
/// entry 1 of `D_actor_451100_8014E6E4` (the sub-model task
/// `func_actor_451100_801330B0`), hands it to `Task_Reparent` with this task
/// and keeps it in `pairTask`, then seeds the animation and runs the step
/// routine once.
///
/// `memCalloc`'s result goes through an untyped `block` that `work` is copied
/// from: the raw pointer is what the `Task::work` store and the null test read,
/// so it stays a short-lived `$v0` quantity while the typed copy takes the
/// callee-saved home it needs across the calls below. Assigning the call result
/// straight to `work` collapses the two into one pseudo and puts `$s1` in all
/// three places.
void func_actor_451100_801328A8(GpEnemy* enemy, Task* task)
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
    func_800B3F84(&work->anim, D_actor_451100_8014E6FC, obj, &work->slots[0x13],
                  work->slots);
    work->animId   = 1;
    work->state    = 2;
    task->msgTable = D_actor_451100_8014E6B4;
    func_actor_451100_80132A1C(task);
    task->state += 1;
}
