#include "common.h"

#include "actors/actor_110800.h"

/// Advances the animation per the work block's `field_474`: step 1 reseeds the
/// slots with a blend, step 2 resets them outright, and either moves on to
/// step 3, which ticks them. The argument is never read.
void func_actor_110800_80132368(Task* task)
{
    if (D_actor_110800_80139F10->field_474 == 1) {
        func_actor_110800_801324AC();
        D_actor_110800_80139F10->field_474 = 3;
        return;
    }
    if (D_actor_110800_80139F10->field_474 == 2) {
        func_actor_110800_80132424();
        D_actor_110800_80139F10->field_474 = 3;
        return;
    }
    if (D_actor_110800_80139F10->field_474 == 3) {
        func_actor_110800_801323DC();
    }
}
