#include "common.h"

#include "actors/actor_210700.h"

/// Message-0x7D4 handler: places the actor. Writes the payload's translation
/// into the root coordinate's local matrix and its Euler angles into the
/// coordinate's `rot` slot, rebuilds the rotation from them, clears `flg` so
/// the world matrix is recomputed, and clears `TmdObject::flags` bit 0x80 to
/// show the model. Always returns 0.
s32 func_actor_210700_8014A344(Task* task, s32 arg1, Actor210700Place* args, s32 arg3)
{
    Actor210700Coord* coord;
    TmdObject*        extra;

    extra             = (TmdObject*)task->extra;
    coord             = (Actor210700Coord*)extra->coords;
    coord->coord.t[0] = args->pos.vx;
    coord->coord.t[1] = args->pos.vy;
    coord->coord.t[2] = args->pos.vz;
    coord->rot.vx     = args->rot.vx;
    coord->rot.vy     = args->rot.vy;
    coord->rot.vz     = args->rot.vz;
    RotMatrix(&coord->rot, &coord->coord);
    coord->flg    = 0;
    extra->flags &= 0xFF7F;
    return 0;
}
