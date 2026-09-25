#include "common.h"

#include "actors/actor_110300.h"
#include "main/task.h"

/// Advances the animation per the work block's `field_474`: step 1 reseeds the
/// slots through `func_800B4114`, step 2 resets them outright, and either moves
/// on to step 3, which ticks them. The argument is never read.
void func_actor_110300_801320C4(GpActorWork* arg0)
{
    if (D_actor_110300_8013A0A0->field_474 == 1) {
        func_actor_110300_80132208();
        D_actor_110300_8013A0A0->field_474 = 3;
        return;
    }
    if (D_actor_110300_8013A0A0->field_474 == 2) {
        func_actor_110300_80132180();
        D_actor_110300_8013A0A0->field_474 = 3;
        return;
    }
    if (D_actor_110300_8013A0A0->field_474 == 3) {
        func_actor_110300_80132138();
    }
}
