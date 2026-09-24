#include "common.h"

#include "actors/actor_421600.h"
#include "actors/actors_shared_80164954.h"
#include "main/gfx.h"
#include "main/tmd.h"

/// Handler for message 0x7D4: write `placement` onto the actor's root
/// coordinate (translation, then yaw / pitch / roll), then read the resulting
/// yaw back out of the matrix's third row and cache it in the work block.
///
/// The `TmdObject` is re-read from `Task::extra` for every access because the
/// stores and the `Gfx_RotMatrix*` calls in between may alias it.
s32 func_actor_421600_8013E52C(Task* task, s32 arg1, ActorShared80164954Placement* placement)
{
    Actor421600Work* work;

    work                                          = (Actor421600Work*)task->work;
    ((TmdObject*)task->extra)->coords->coord.t[0] = placement->pos.vx;
    ((TmdObject*)task->extra)->coords->coord.t[1] = placement->pos.vy;
    ((TmdObject*)task->extra)->coords->coord.t[2] = placement->pos.vz;
    Gfx_RotMatrixY(&((TmdObject*)task->extra)->coords->coord, placement->rot.vy, 1);
    Gfx_RotMatrixX(&((TmdObject*)task->extra)->coords->coord, placement->rot.vx, 0);
    Gfx_RotMatrixZ(&((TmdObject*)task->extra)->coords->coord, placement->rot.vz, 0);
    ((TmdObject*)task->extra)->coords->flg = 0;
    work->field_16                         = ratan2(-((TmdObject*)task->extra)->coords->coord.m[2][0],
                                                    ((TmdObject*)task->extra)->coords->coord.m[2][2]);
    return 1;
}
