#include "common.h"

#include "actors/actor_160700.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/gameplay.h"
#include "main/task.h"
#include "main/tmd.h"

/// State-1 handler of the actor's dispatcher: recomputes the root part's
/// world matrix, hands the position 800 units above it to the model's
/// light/colour step, then runs the animation step and draws the shadow.
void func_actor_160700_80132390(GpEnemy* enemy, Task* task)
{
    TmdObject*     obj;
    GsCOORDINATE2* coord;
    VECTOR         vec;

    obj   = task->extra;
    coord = obj->coords;
    Gp_UpdateCoord(coord);
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1] - 800;
    vec.vz = coord->workm.t[2];
    func_800D7A9C(obj, &vec, 0, 3);
    func_actor_160700_80132184(task);
    func_actor_160700_8013243C(task);
}
