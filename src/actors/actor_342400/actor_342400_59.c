#include "common.h"

#include "main/task.h"

#include "actors/actor_342400.h"

/// While `field_41E` is 1, consumes the pending request in `field_448`:
/// request 3 moves the state machine to state 8 and request 5 to state 9,
/// anything else is just cleared. Returns 1 when `field_41E` is 1 and 0
/// otherwise.
s32 func_actor_342400_8016BEF0(Task* arg0)
{
    Actor342400Work* work = (Actor342400Work*)arg0->work;

    if (work->field_41E == 1) {
        switch (work->field_448) {
            case 3:
                work->field_420 = 8;
                work->field_422 = 0;
                break;
            case 5:
                work->field_420 = 9;
                work->field_422 = 0;
                break;
        }
        work->field_448 = 0;
        return 1;
    }
    return 0;
}
