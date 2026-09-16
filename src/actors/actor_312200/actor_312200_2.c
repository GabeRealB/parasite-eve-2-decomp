#include "common.h"

#include "actors/actor_312200.h"
#include "main/gfx.h"
#include "main/tmd.h"

/// Same placement opcode as `ActorsShared80169f74` (X then Y then Z, re-fetch
/// the coordinate for every field, then `ratan2` of the matrix Z-axis), except
/// the heading is cached at `Actor312200Work::yaw` (0x08) rather than 0x16.
s32 func_actor_312200_801635CC(Task* task, s32 arg1, ActorShared80169f74Placement* placement)
{
    GsCOORDINATE2*   coord;
    s32              mx;
    s32              mz;
    Actor312200Work* work;

    work                                           = (Actor312200Work*)task->idMap;
    ((TmdObject*)task->extra)->field_8->coord.t[0] = placement->pos.vx;
    ((TmdObject*)task->extra)->field_8->coord.t[1] = placement->pos.vy;
    ((TmdObject*)task->extra)->field_8->coord.t[2] = placement->pos.vz;
    Gfx_RotMatrixX(&((TmdObject*)task->extra)->field_8->coord, placement->rot.vx, 1);
    Gfx_RotMatrixY(&((TmdObject*)task->extra)->field_8->coord, placement->rot.vy, 0);
    Gfx_RotMatrixZ(&((TmdObject*)task->extra)->field_8->coord, placement->rot.vz, 0);
    ((TmdObject*)task->extra)->field_8->flg = 0;
    coord                                   = ((TmdObject*)task->extra)->field_8;
    mx                                      = coord->coord.m[2][0];
    mz                                      = coord->coord.m[2][2];
    work->yaw                               = ratan2(-mx, mz);
    return 1;
}

INCLUDE_ASM("actors/nonmatchings/actor_312200/actor_312200_2", func_actor_312200_801636CC);

INCLUDE_ASM("actors/nonmatchings/actor_312200/actor_312200_2", func_actor_312200_80163778);

/// Per-tick state callback. A live actor (`field_4`) re-enters work state 2
/// with the 0x896 timer armed at 0x10; once the 0x892 timer has counted those
/// 0x10 ticks and the task's flag bit 0 is set, the state drops to 1 and the
/// timer to 4. Either way the tick ends in the actor's anim/particle update.
void func_actor_312200_801637CC(Task* task)
{
    Actor312200Work* work;

    work = (Actor312200Work*)task->idMap;
    if (work->field_4 != 0) {
        work->field_88C = 2;
        work->field_896 = 0x10;
        func_actor_312200_80162FB4(task);
    }
    if (work->field_892 == 0x10 && (work->field_5C & 1)) {
        work->field_892 = 4;
        work->field_88C = 1;
    }
    func_actor_312200_80162FB4(task);
}
