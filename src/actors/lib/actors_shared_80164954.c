#include "common.h"

#include "actors/actors_shared_80164954.h"
#include "actors/actors_shared_80164af0.h"

#include "main/gfx.h"
#include "main/tmd.h"

/// Placement opcode shared by `actor_323000`, `actor_323400` and
/// `actor_421600`: `placement` is written straight onto the actor's root
/// coordinate frame, then the resulting matrix's own yaw is read back out of
/// its third row with `ratan2` and cached in the work block, so the walk that
/// follows turns from where the placement actually left the actor.
///
/// The `TmdObject` is re-read from `Task::extra` for every access because the
/// stores and the `Gfx_RotMatrix*` calls in between may alias it.
s32 ActorsShared80164954(Task* task, s32 arg1, ActorShared80164954Placement* placement)
{
    ActorShared80164af0Work* work;

    work                                           = (ActorShared80164af0Work*)task->idMap;
    ((TmdObject*)task->extra)->field_8->coord.t[0] = placement->pos.vx;
    ((TmdObject*)task->extra)->field_8->coord.t[1] = placement->pos.vy;
    ((TmdObject*)task->extra)->field_8->coord.t[2] = placement->pos.vz;
    Gfx_RotMatrixY(&((TmdObject*)task->extra)->field_8->coord, placement->rot.vy, 1);
    Gfx_RotMatrixX(&((TmdObject*)task->extra)->field_8->coord, placement->rot.vx, 0);
    Gfx_RotMatrixZ(&((TmdObject*)task->extra)->field_8->coord, placement->rot.vz, 0);
    ((TmdObject*)task->extra)->field_8->flg = 0;
    work->field_16                          = ratan2(-((TmdObject*)task->extra)->field_8->coord.m[2][0],
                                                     ((TmdObject*)task->extra)->field_8->coord.m[2][2]);
    return 1;
}
