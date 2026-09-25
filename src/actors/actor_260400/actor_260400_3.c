#include "common.h"

#include "actors/actor_260400.h"
#include "gameplay/3A34.h"
#include "main/task.h"
#include "main/tmd.h"

/// State handler of the helper tasks the actor's spawn table starts: the first
/// tick hangs the task's own coordinate frame off the actor's part
/// `spawnArg1`, shows its model and steps to state 1; every later tick relights
/// the model from a point 0x320 above the actor's root translation.
void func_actor_260400_8014A6F8(Task* task)
{
    TmdObject*     extra = task->extra;
    GsCOORDINATE2* coord = extra->coords;
    GsCOORDINATE2* parts = ((TmdObject*)D_actor_260400_80154C74->extra)->coords;
    GsCOORDINATE2* part  = parts + task->spawnArg1;
    VECTOR         vec;

    switch (task->state) {
        case 0:
            coord->flg   = 0;
            extra->flags = 0;
            coord->sub   = part;
            task->state++;
            break;
        case 1:
            vec.vx = parts->workm.t[0];
            vec.vy = parts->workm.t[1] - 0x320;
            vec.vz = parts->workm.t[2];
            func_800D7A9C(extra, &vec, 0, 3);
            break;
    }
}
