#include "common.h"
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "actors/actors_shared_8013411c.h"
#include "gameplay/1BC.h"
#include "main/task.h"
#include "main/tmd.h"

/// Scene-script handler that places the task's model: `placement`'s position
/// becomes the translation of the `TmdObject`'s first coordinate, its angles
/// are applied Y, then X, then Z, and the coordinate is marked dirty.
void func_actor_121300_8013411C(Task* task, s32 arg1, ActorShared8013411cPlacement* placement)
{
    GsCOORDINATE2* coord;
    MATRIX*        mtx;

    coord             = ((TmdObject*)task->extra)->coords;
    coord->coord.t[0] = placement->pos.vx;
    coord->coord.t[1] = placement->pos.vy;
    mtx               = &coord->coord;
    coord->coord.t[2] = placement->pos.vz;
    Gfx_RotMatrixY(mtx, placement->rot.vy, 1);
    Gfx_RotMatrixX(mtx, placement->rot.vx, 0);
    Gfx_RotMatrixZ(mtx, placement->rot.vz, 0);
    coord->flg = 0;
}
