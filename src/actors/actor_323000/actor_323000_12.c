#include "common.h"

#include <psyq/libgte.h>

#include "actors/actor_323000.h"
#include "actors/actors_shared_80164954.h"
#include "main/gfx.h"
#include "main/tmd.h"

/// Handler for message 0x7D4: writes `placement` onto the actor's root
/// coordinate (translation, then yaw / pitch / roll), then reads the resulting
/// yaw back out of the matrix's third row and caches it in `field_16`.
///
/// The `TmdObject` is re-read from `Task::extra` for every access because the
/// stores and the `Gfx_RotMatrix*` calls in between may alias it.
s32 func_actor_323000_80164954(Task* task, s32 arg1, ActorShared80164954Placement* placement)
{
    Actor323000Work* work;

    work                                          = (Actor323000Work*)task->work;
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
