#include "common.h"

#include "gameplay/3A34.h"
#include "main/task.h"
#include "main/tmd.h"

#include "actors/actors_shared_801330ac.h"

void func_actor_450800_80132E9C(void* enemy, Task* task);
void func_actor_450800_801332B8(void* enemy, Task* task);
void func_actor_450800_80133364(Task* task);

void Gp_UpdateCoord(GsCOORDINATE2* arg0);

void func_actor_450800_80133264(Task* task)
{
    void (*fns[2])(void*, Task*) = { func_actor_450800_80132E9C, func_actor_450800_801332B8 };

    fns[task->state](task->spawnArg2, task);
}

/// Per-frame handler of the enemy this actor's model task carries: state 1 of
/// `func_actor_450800_80133264`'s `fns` table, and the twin of
/// `ActorsShared80131e24Sub1` - the same body, with this overlay's own
/// animation tick and shadow routine in its two trailing calls.
///
/// Refreshes the enemy model root's coordinate, feeds its world translation
/// (lowered by 800 on y, to sit on the ground) to `func_800D7A9C`, then ticks
/// the enemy's animation state through `ActorsShared801330ac` and draws its
/// ground shadow.
void func_actor_450800_801332B8(void* enemy, Task* task)
{
    TmdObject*     obj;
    GsCOORDINATE2* coord;
    VECTOR         pos;

    obj   = task->extra;
    coord = obj->coords;
    Gp_UpdateCoord(coord);
    pos.vx = coord->workm.t[0];
    pos.vy = coord->workm.t[1] - 800;
    pos.vz = coord->workm.t[2];
    func_800D7A9C(obj, &pos, 0, 3);
    ActorsShared801330ac(task);
    func_actor_450800_80133364(task);
}
