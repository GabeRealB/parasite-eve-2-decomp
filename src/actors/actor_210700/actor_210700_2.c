#include "common.h"

#include "actors/actor_210700.h"

#include "gameplay/1BC.h"
#include "gameplay/3CD8.h"
#include "gameplay/gameplay.h"

#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"

void Gp_DrawEffGroundQuad(VECTOR3* arg0, s32 arg1, s16 arg2);

/// Spawn state: allocates the zeroed work block into `Task::work` (handing
/// the task to `Gp_EnemyTaskExit` if that fails), marks no animation loaded,
/// hides the model with `TmdObject::flags` bit 0x80, then runs its own 0x7D4
/// and 0x7D3 message handlers directly to place the actor at the origin -
/// which shows it again - and start animation 1 of source 0. It draws the
/// ground shadow under the model's second part, points the model at the work
/// block's light / colour matrices, installs the message table and the exit
/// callback, and advances to the tick state.
void func_actor_210700_80149F90(Task* task)
{
    Actor210700Work* work;
    TmdObject*       extra;
    Actor210700Place args;
    Actor210700Anim  anim;
    VECTOR3          pos;

    extra = (TmdObject*)task->extra;
    work  = (Actor210700Work*)memCalloc(0x540, 0);
    if (work == NULL) {
        Gp_EnemyTaskExit(task);
        return;
    }
    task->work      = (TaskIdMap*)work;
    work->field_478 = -1;
    work->field_47C = -1;
    work->field_53E = -1;
    extra->flags    = 0x80;
    args.pos.vx     = 0;
    args.pos.vy     = 0;
    args.pos.vz     = 0;
    args.rot.vx     = 0;
    args.rot.vy     = 0;
    args.rot.vz     = 0;
    func_actor_210700_8014A344(task, 0x7D4, &args, 0);
    anim.field_0 = 0;
    anim.field_4 = 1;
    anim.field_8 = 0;
    func_actor_210700_8014A224(task, 0x7D3, &anim, 0);
    if (func_800EA1A8((VECTOR3*)((TmdObject*)task->extra)->coords[1].workm.t, &pos) != 0) {
        Gp_DrawEffGroundQuad(&pos, 0x400, Gp_State1C->groundShade);
    }
    func_actor_210700_8014A208(task);
    task->msgTable     = D_actor_210700_801585D8;
    task->exitCallback = func_actor_210700_8014A1E8;
    task->state++;
}

/// Tick state: while an animation is running ticks slots 1..0x13 and draws
/// the ground shadow under the model's second part. While the game session's
/// view is ready it invalidates and rebuilds that part's coordinate and hands
/// it to `func_800D7A9C`. It then runs the texture-upload step and counts
/// `field_53E` down, freeing the model buffers on the frame it reaches 0.
void func_actor_210700_8014A0AC(Task* task)
{
    Actor210700Work* work;
    TmdObject*       ext;
    VECTOR3          pos;
    s16              count;
    s32              i;

    work = (Actor210700Work*)task->work;
    ext  = task->extra;
    if (work->field_474 != 0) {
        for (i = 1; i < 0x14; i++) {
            Gp_AnimTickIndex(&work->anim, i);
        }
    }
    if (func_800EA1A8((VECTOR3*)((TmdObject*)task->extra)->coords[1].workm.t, &pos) != 0) {
        Gp_DrawEffGroundQuad(&pos, 0x400, Gp_State1C->groundShade);
    }
    if (gGameSession->viewReady != 0) {
        ((TmdObject*)task->extra)->coords[1].flg = 0;
        Gp_UpdateCoord(&((TmdObject*)task->extra)->coords[1]);
        func_800D7A9C(ext, (VECTOR*)((TmdObject*)task->extra)->coords[1].workm.t, 0, 3);
    }
    func_actor_210700_80149E30((GpActorWork*)task);
    count = work->field_53E;
    if (count >= 0) {
        if (count == 0) {
            Tmd_FreeBuffers(ext);
        }
        work->field_53E = (s16)((u16)work->field_53E - 1);
    }
}
