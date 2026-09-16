#include "common.h"

#include "actors/actor_210700.h"
#include "actors/actors_shared_801327b4.h"
#include "actors/actors_shared_801334c4.h"

#include "gameplay/1BC.h"
#include "gameplay/3CD8.h"

#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"

void Gp_DrawEffGroundQuad(VECTOR3* arg0, s32 arg1, s16 arg2);

/// Spawn handler: allocates the 0x540-byte work block and parks it in the
/// task's `idMap` slot, seeds its head, shows the model by setting
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
    task->idMap     = (TaskIdMap*)work;
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

INCLUDE_ASM("actors/nonmatchings/actor_210700/actor_210700_2", func_actor_210700_8014A0AC);
