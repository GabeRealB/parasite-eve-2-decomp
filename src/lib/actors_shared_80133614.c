#include "common.h"

#include "actors/actor_444000.h"
#include "actors/actors_shared_80132808.h"
#include "gameplay/1BC.h"
#include "main/gfx.h"
#include "main/task.h"
#include "main/tmd.h"

void ActorsShared80133614(Actor444000* task, s16 arg1)
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

    ((TmdObject*)task->extra)->coords[3].flg = 0;
    Gp_UpdateCoord(&((TmdObject*)task->extra)->coords[3]);
    ActorsShared80132808(&((TmdObject*)task->extra)->coords[3], work->field_7C8);
    ((TmdObject*)task->extra)->coords[3].flg = 0;
    Gp_UpdateCoord(&((TmdObject*)task->extra)->coords[3]);
    ((TmdObject*)work->field_ECC[4]->task->extra)->coords[0].flg = 0;
    Gp_UpdateCoord(&((TmdObject*)work->field_ECC[4]->task->extra)->coords[0]);
    ((TmdObject*)task->extra)->coords[4].flg = 0;
    Gp_UpdateCoord(&((TmdObject*)task->extra)->coords[4]);
}
