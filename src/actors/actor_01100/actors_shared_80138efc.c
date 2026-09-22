#include "common.h"

#include "actors/actors_shared_80138efc.h"

/// Arms the motion pair for the current sub-state when `field_BA8` is still
/// clear, and switches to state 0xF when `field_BA9` is set.
void ActorsShared80138efc(GpEnemy* enemy, Task* task, ActorsShared80138efcWork* work)
{
    if (work->field_BA8 == 0) {
        if (work->field_BAE == 0) {
            work->field_BA4 = 0xB;
        } else {
            work->field_BA4 = 0xE;
        }
        work->field_BA5 = 1;
        work->field_BA8 = (u8)work->field_BA8 + 1;
    }
    if (work->field_BA9 != 0) {
        work->field_BA6 = 1;
        work->field_B9C = 0;
        work->field_BAB = 0;
        work->state     = 0xF;
        work->field_BA8 = 0;
    }
}
