#include "common.h"

#include "actors/actor_260500.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "main/task.h"
#include "main/tmd.h"

/// Per-frame state (state 1 of `func_actor_260500_8014A460`): refreshes the
/// model root's world matrix, relights the model from a point 0x320 above its
/// translation, then runs the update and draws the ground shadow.
void func_actor_260500_8014A4BC(GpEnemy* enemy, Task* task)
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
    func_actor_260500_8014A110(task);
    func_actor_260500_8014A99C(task);
}
