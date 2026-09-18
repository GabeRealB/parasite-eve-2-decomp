#include "common.h"

#include "actors/actor_161500.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/gameplay.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"

void ActorsShared80132378(Task* task);

/// The actor's draw body: refreshes the model root's coordinate, lights the
/// model at its world translation raised by 800 on y, then runs the step body.
/// `field_4F0` is the head-tracking blend rate handed to `func_800B0928`,
/// ramped toward 0x1000 in 0x200 steps while `field_4EE` is 1 and back down to
/// 0 otherwise, so the actor turns its head to the player and away again
/// smoothly instead of snapping.
void ActorsShared80131e24Sub1(GpEnemy* enemy, Task* task)
{
    TmdObject*       obj;
    GsCOORDINATE2*   coord;
    Actor161500Work* work;
    VECTOR           pos;

    obj   = (TmdObject*)task->extra;
    coord = obj->field_8;
    work  = (Actor161500Work*)task->work;
    Gp_UpdateCoord(coord);
    pos.vx = coord->workm.t[0];
    pos.vy = coord->workm.t[1] - 0x320;
    pos.vz = coord->workm.t[2];
    func_800D7A9C(obj, &pos, 0, 3);
    func_actor_161500_8013252C(task);
    if (work->field_4EE == 1) {
        work->field_4F0 += 0x200;
        if (work->field_4F0 > 0x1000) {
            work->field_4F0 = 0x1000;
        }
    } else {
        work->field_4F0 -= 0x200;
        if (work->field_4F0 < 0) {
            work->field_4F0 = 0;
        }
    }
    func_800B0928(task, Game_GetPtrSlot(3), 0x200, 0x100, work->field_4F0);
    ActorsShared80132378(task);
}
