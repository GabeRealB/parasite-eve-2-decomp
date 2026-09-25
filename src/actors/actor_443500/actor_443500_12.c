#include "common.h"

#include <psyq/libgte.h>

#include "main/task.h"
#include "main/tmd.h"

/// Argument block of message 0x7D4: a world translation followed by the Euler
/// angles handed to `RotMatrix`.
typedef struct Actor443500PlaceArgs {
    /* 0x00 */ VECTOR  pos;
    /* 0x10 */ SVECTOR rot;
} Actor443500PlaceArgs;
STATIC_ASSERT_SIZEOF(Actor443500PlaceArgs, 0x18);

/// `GsCOORDINATE2` at `TmdObject::coords`, with the Euler angles kept in the
/// slot libgs names `param` (0x44), from which the rotation is rebuilt.
typedef struct Actor443500Coord {
    /* 0x00 */ s32     flg;
    /* 0x04 */ MATRIX  coord;
    /* 0x24 */ MATRIX  workm;
    /* 0x44 */ SVECTOR rot;
} Actor443500Coord;
STATIC_ASSERT_SIZEOF(Actor443500Coord, 0x4C);

/// Message-0x7D4 handler: places the actor at `args` - the translation goes
/// straight into the root coordinate's local matrix, the Euler angles into the
/// coordinate's `rot` slot, from which the rotation is rebuilt. Clearing `flg`
/// has the world matrix recomputed. Returns 0.
s32 func_actor_443500_80132900(Task* task, s32 arg1, Actor443500PlaceArgs* args)
{
    Actor443500Coord* coord;

    coord             = (Actor443500Coord*)((TmdObject*)task->extra)->coords;
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
