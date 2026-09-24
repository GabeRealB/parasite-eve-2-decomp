#include "common.h"
#include <psyq/libgte.h>

#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/gameplay.h"

#include "actors/actor_215100.h"

/// State-1 handler of the actor's dispatcher: recomputes the root part's
/// world matrix, hands the position 800 units above it to the model's
/// light/colour step, then runs the animation step and draws the shadow.
void func_actor_215100_8014CA80(GpEnemy* enemy, Task* task)
{
    TmdObject*     obj;
    GsCOORDINATE2* coord;
    VECTOR         pos;

    obj   = (TmdObject*)task->extra;
    coord = obj->coords;
    Gp_UpdateCoord(coord);
    pos.vx = coord->workm.t[0];
    pos.vy = coord->workm.t[1] - 0x320;
    pos.vz = coord->workm.t[2];
    func_800D7A9C(obj, &pos, 0, 3);
    func_actor_215100_8014C874(task);
    func_actor_215100_8014CB2C(task);
}
