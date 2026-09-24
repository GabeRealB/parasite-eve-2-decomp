#include "common.h"

#include "main/gfx.h"
#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/gameplay.h"

#include "actors/actor_342400.h"

/// Moves the model so that part `arg1` lands on `arg2`: shifts the root
/// translation by the part's offset from the root in view space and marks
/// the part's coordinate dirty.
void func_actor_342400_80169654(Task* arg0, s16 arg1, SVECTOR3* arg2)
{
    MATRIX         local;
    MATRIX         world;
    GsCOORDINATE2* coord;
    GsCOORDINATE2* coords;

    coords = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->coords;
    coord  = &coords[arg1];
    Gp_UpdateCoord(coord);
    Gp_WorldToLocal(&Gfx_ViewWorldMtx, &coords->workm, &local);
    Gp_WorldToLocal(&Gfx_ViewWorldMtx, &coord->workm, &world);
    coords->coord.t[0] = arg2->vx - (world.t[0] - local.t[0]);
    coords->coord.t[1] = arg2->vy - (world.t[1] - local.t[1]);
    coords->coord.t[2] = arg2->vz - (world.t[2] - local.t[2]);
    coord->flg         = 0;
}

/// Scales `arg1` by the animation speed `field_41C`, in 1/16 units.
s32 func_actor_342400_80169728(Task* arg0, s16 arg1)
{
    return (s32)((((Actor342400Work*)arg0->work)->field_41C * arg1) << 0xC) >> 0x10;
}
