#include "common.h"

#include <psyq/libgte.h>

#include "actors/actor_310600.h"
#include "main/task.h"
#include "main/tmd.h"

/// Placement block `func_actor_310600_80162C18` receives: a world translation
/// followed by the Euler angles handed to `RotMatrix`.
typedef struct Actor310600Placement {
    /* 0x00 */ VECTOR  pos;
    /* 0x10 */ SVECTOR rot;
} Actor310600Placement;
STATIC_ASSERT_SIZEOF(Actor310600Placement, 0x18);

/// Places the actor at `args`: the translation goes straight into the root
/// part's local matrix, the Euler angles into the coordinate's `rot` slot, and
/// the rotation is rebuilt from them. Clearing `flg` makes `_gpUpdateCoordTree`
/// recompute the world matrix. `arg1` is unused.
s32 func_actor_310600_80162C18(Task* task, s32 arg1, Actor310600Placement* args)
{
    Actor310600Coord* coord;

    coord             = (Actor310600Coord*)((TmdObject*)task->extra)->coords;
    coord->coord.t[0] = args->pos.vx;
    coord->coord.t[1] = args->pos.vy;
    coord->coord.t[2] = args->pos.vz;
    coord->rot.vx     = args->rot.vx;
    coord->rot.vy     = args->rot.vy;
    coord->rot.vz     = args->rot.vz;
    RotMatrix(&coord->rot, &coord->coord);
    coord->flg = 0;
    return 0;
}
