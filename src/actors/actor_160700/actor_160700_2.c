#include "common.h"

#include "actors/actors_shared_8014c874.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/gameplay.h"
#include "main/task.h"
#include "main/tmd.h"

void ActorsShared80132378(Task* task);

/// State-1 handler: recomputes the part coordinate's world matrix, lifts its
/// translation by 800 and hands it to the model's light/colour step, then runs
/// the shared step and shadow bodies.
void ActorsShared80131e24Sub1(GpEnemy* enemy, Task* task)
{
    TmdObject*     obj;
    GsCOORDINATE2* coord;
    VECTOR         pos;

    obj   = (TmdObject*)task->extra;
    coord = obj->field_8;
    Gp_UpdateCoord(coord);
    pos.vx = coord->workm.t[0];
    pos.vy = coord->workm.t[1] - 0x320;
    pos.vz = coord->workm.t[2];
    func_800D7A9C(obj, &pos, 0, 3);
    ActorsShared8014c874(task);
    ActorsShared80132378(task);
}
