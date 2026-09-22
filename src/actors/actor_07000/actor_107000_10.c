#include "common.h"

#include "actors/actors_shared_80136614.h"
#include "gameplay/1BC.h"
#include "main/task.h"
#include "main/tmd.h"

extern TaskDesc Actor07000_D0D7D0;

void Actor07000_Fn062A8(Task* arg0)
{
    SVECTOR        offset;
    u32            dist;
    GsCOORDINATE2* coords;
    GsCOORDINATE2* child;
    Task*          task;
    s32            angle;

    coords    = ((TmdObject*)arg0->extra)->coords;
    child     = &coords[1];
    angle     = ActorsShared80136614(coords, &dist);
    offset.vz = 0;
    offset.vy = 0;
    offset.vx = 0;
    task      = Task_SpawnFromTable(&Actor07000_D0D7D0, 1, angle, 0);
    if (task != NULL) {
        Gp_CopyCoordOffset(task, child, &offset);
        Task_Reparent(arg0, task);
    }
}
