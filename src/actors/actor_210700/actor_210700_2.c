#include "common.h"

#include "actors/actor_210700.h"
#include "actors/actors_shared_801327b4.h"
#include "actors/actors_shared_801334c4.h"

#include "gameplay/1BC.h"
#include "gameplay/3CD8.h"
#include "gameplay/gameplay.h"

#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"

void Gp_DrawEffGroundQuad(VECTOR3* arg0, s32 arg1, s16 arg2);

/// Spawn handler: allocates the 0x540-byte work block and parks it in the
/// task's `work` slot, seeds its head, shows the model by setting
/// `TmdObject::field_C` bit 0x80, sends the two script commands that place the
/// actor (0x7D4 at the origin) and start its animation (0x7D3), draws the
/// ground shadow under the model's second part, republishes the light/colour
/// pair from the work block, installs the overlay's command table and the
/// shared exit callback, then advances to the tick handler.
///
/// The engine hands the task to `Gp_EnemyTaskExit` and gives up when the
/// allocation fails.
void func_actor_210700_80149F90(Task* task)
{
    Actor210700Work*         work;
    TmdObject*               extra;
    ActorsShared801334c4Args args;
    Actor210700Anim          anim;
    VECTOR3                  pos;

    extra = (TmdObject*)task->extra;
    work  = (Actor210700Work*)Mem_Calloc(0x540, 0);
    if (work == NULL) {
        Gp_EnemyTaskExit(task);
        return;
    }
    task->work      = (TaskIdMap*)work;
    work->field_478 = -1;
    work->field_47C = -1;
    work->field_53E = -1;
    extra->field_C  = 0x80;
    args.pos.vx     = 0;
    args.pos.vy     = 0;
    args.pos.vz     = 0;
    args.rot.vx     = 0;
    args.rot.vy     = 0;
    args.rot.vz     = 0;
    ActorsShared801334c4(task, 0x7D4, &args, 0);
    anim.field_0 = 0;
    anim.field_4 = 1;
    anim.field_8 = 0;
    func_actor_210700_8014A224(task, 0x7D3, &anim, 0);
    if (func_800EA1A8((VECTOR3*)((TmdObject*)task->extra)->field_8[1].workm.t, &pos) != 0) {
        Gp_DrawEffGroundQuad(&pos, 0x400, Gp_State1C->field_8);
    }
    func_actor_210700_8014A208(task);
    task->field_24     = D_actor_210700_801585D8;
    task->exitCallback = ActorsShared801327b4;
    task->state++;
}

/// Per-frame tick of the actor: while the work block's `field_474` is set,
/// ticks animation slots 1..0x13 off the context at the head of the block, then
/// draws the ground shadow under the model's second part. While the session's
/// `field_4D` is set it republishes that second part's coordinate -- clearing
/// its `flg` to invalidate the cached matrix, rebuilding it from its `workm`
/// and handing it to `func_800D7A9C` -- which is the same block
/// `func_actor_335800_80163568` and `func_actor_361100_801631A4` run against
/// their own child part. The overlay's texture-upload handler runs next, and
/// `field_53E` counts down to the frame its zero value frees the model buffers.
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
            Gp_AnimTickIndex((GpAnimCtx*)work, i);
        }
    }
    if (func_800EA1A8((VECTOR3*)((TmdObject*)task->extra)->field_8[1].workm.t, &pos) != 0) {
        Gp_DrawEffGroundQuad(&pos, 0x400, Gp_State1C->field_8);
    }
    if (gGameSession->field_4D != 0) {
        ((TmdObject*)task->extra)->field_8[1].flg = 0;
        Gp_UpdateCoord(&((TmdObject*)task->extra)->field_8[1]);
        func_800D7A9C(ext, (VECTOR*)((TmdObject*)task->extra)->field_8[1].workm.t, 0, 3);
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
