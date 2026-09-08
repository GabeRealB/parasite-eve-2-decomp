#include "common.h"

#include "actors/actors_shared_8013587c.h"

/// Per-frame tick. State 0 waits for `Gp_TickObjFlag2` on the spawn block to
/// fire, then selects animation 0x13, clears `field_6E0` and advances to state
/// 1. State 1 waits for `field_698` to reach 0x3B and drops back to state 0
/// with animation 2.
void ActorsShared8013587c(Task* task)
{
    ActorsShared8013587cWork* work;
    s16                       state;

    work  = (ActorsShared8013587cWork*)task->idMap;
    state = work->field_6A8;
    switch (state) {
        case 0:
            if (Gp_TickObjFlag2((GpObj5D*)task->spawnArg2) != 0) {
                work->field_694 = 0x13;
                work->field_6A8 = 1;
                work->field_6E0 = 0;
            }
            break;
        case 1:
            if (work->field_698 >= 0x3B) {
                work->field_694 = 2;
                work->field_6A6 = 2;
                work->field_6A8 = 0;
            }
            break;
    }
}
