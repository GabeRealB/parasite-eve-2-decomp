#include "common.h"

#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/1BC.h"

#include "actors/actor_104600.h"
#include "actors/actors_shared_8014a1c4.h"

void ActorsShared8014ae70(Task* task)
{
    Actor104600Work* work;

    work = (Actor104600Work*)task->work;
    switch (work->field_286) {
        case 0:
            ActorsShared8014a1c4(task);
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
