#include "common.h"

#include "actors/actor_444000.h"
#include "gameplay/1BC.h"
#include "main/task.h"
#include "main/tmd.h"

INCLUDE_ASM("actors/nonmatchings/actor_444000/actor_444000_3", func_actor_444000_80133010);

/// Walk the actor's stored yaw toward `arg1`, moving at most 0x71 per call and
/// clamping the request to +/-0x200, then push the new angle onto the model:
/// part 3's coordinate is rebuilt twice around the yaw helper, the fifth
/// escort's root coordinate is refreshed, and part 4 follows.
void func_actor_444000_80133C58(Actor444000* task, s16 arg1)
{
    Actor444000Work* work = task->field_1C;
    s16              value;

    value = arg1;
    if (arg1 > 0x200) {
        value = 0x200;
    }
    if (arg1 < -0x200) {
        value = -0x200;
    }

    if (work->field_7C8 < value) {
        if (value - work->field_7C8 >= 0x72) {
            work->field_7C8 = work->field_7C8 + 0x71;
        } else {
            work->field_7C8 = value;
        }
    } else if (value < work->field_7C8) {
        if (abs(work->field_7C8 - value) >= 0x72) {
            work->field_7C8 = work->field_7C8 - 0x71;
        } else {
            work->field_7C8 = value;
        }
    }

    ((TmdObject*)task->extra)->field_8[3].flg = 0;
    Gp_UpdateCoord(&((TmdObject*)task->extra)->field_8[3]);
    func_actor_444000_80132808(&((TmdObject*)task->extra)->field_8[3], work->field_7C8);
    ((TmdObject*)task->extra)->field_8[3].flg = 0;
    Gp_UpdateCoord(&((TmdObject*)task->extra)->field_8[3]);
    ((TmdObject*)work->field_ECC[4]->task->extra)->field_8[0].flg = 0;
    Gp_UpdateCoord(&((TmdObject*)work->field_ECC[4]->task->extra)->field_8[0]);
    ((TmdObject*)task->extra)->field_8[4].flg = 0;
    Gp_UpdateCoord(&((TmdObject*)task->extra)->field_8[4]);
}
