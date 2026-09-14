#include "common.h"

#include "actors/actor_521100.h"

/// Main-executable global with no module header yet: the remaining-enemy count.
extern s16 D_80073BA0;

INCLUDE_ASM("actors/nonmatchings/actor_521100/actor_521100_4", func_actor_521100_80135B40);

INCLUDE_ASM("actors/nonmatchings/actor_521100/actor_521100_4", func_actor_521100_80135B80);

s32 func_actor_521100_80135BEC(Actor521100* arg0)
{
    if (D_80073BA0 > 0) {
        arg0->field_1C->field_6A8 = 1;
    }
    return 0;
}

INCLUDE_ASM("actors/nonmatchings/actor_521100/actor_521100_4", func_actor_521100_80135C14);
