#include "common.h"

#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/1BC.h"

#include "actors/actor_104600.h"

/// Per-frame tick of the actor's four-way state machine. State 0 hands the
/// frame to `func_actor_104600_80134EC8` and state 2 idles. State 3 clears the
/// two flags at 0x292 / 0x2A6 and counts frames: every fourth one it arms
/// `field_28E`, clears `field_290` and restarts the count. Whatever the count,
/// state 3 drops back to state 0 once `Gp_TickObjFlag2` fires on the spawn
/// block.
void func_actor_104600_80135B74(Task* task)
{
    Actor104600Work* work;

    work = (Actor104600Work*)task->work;
    switch (work->field_286) {
        case 0:
            func_actor_104600_80134EC8(task);
            break;
        case 2:
            break;
        case 3:
            work->field_292 = 0;
            work->field_2A6 = 0;
            work->field_28A = work->field_28A + 1;
            if (work->field_28A >= 4) {
                work->field_28E = 1;
                work->field_290 = 0;
                work->field_28A = 0;
            }
            if (Gp_TickObjFlag2((GpObj5D*)task->spawnArg2) != 0) {
                work->field_286 = 0;
            }
            break;
    }
}
