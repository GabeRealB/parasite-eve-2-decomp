#include "common.h"

#include "main/gfx.h"
#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/gameplay.h"

#include "actors/actor_405800.h"

void func_actor_405800_80138514(Task* arg0, s16 arg1, Actor405800ViewPos* arg2)
{
    MATRIX         root;
    MATRIX         local;
    GsCOORDINATE2* coord;
    GsCOORDINATE2* coords;

    coords            = (GsCOORDINATE2*)((TmdObject*)arg0->extra)->coords;
    gGfxViewCoord.flg = 0;
    coord             = &coords[arg1];
    Gp_UpdateCoord(&gGfxViewCoord);
    coords[0].flg = 0;
    coord->flg    = 0;
    Gp_UpdateCoord(coord);
    Gp_WorldToLocal(&gGfxViewCoord.workm, &coords[0].workm, &root);
    Gp_WorldToLocal(&gGfxViewCoord.workm, &coord->workm, &local);
    coords[0].coord.t[0] = arg2->x - (local.t[0] - root.t[0]);
    coords[0].coord.t[2] = arg2->z - (local.t[2] - root.t[2]);
    coords[0].flg        = 0;
    coord->flg           = 0;
    Gp_UpdateCoord(coord);
}
