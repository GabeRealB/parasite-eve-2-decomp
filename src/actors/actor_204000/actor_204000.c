#include "common.h"
#include "actors/actor_104000.h"

INCLUDE_ASM("actors/nonmatchings/actor_204000/actor_204000", func_actor_204000_8014A074);

INCLUDE_ASM("actors/nonmatchings/actor_204000/actor_204000", func_actor_204000_8014A5B8);

INCLUDE_RODATA("actors/nonmatchings/actor_204000/actor_204000", D_actor_204000_80149E20);

INCLUDE_ASM("actors/nonmatchings/actor_204000/actor_204000", func_actor_204000_8014A75C);

/// Seeds animation slots 1..5 with the step's animation id and blend speed.
static __inline__ void Actor204000_ResetSlots(Actor104000Work* arg0)
{
    Actor104000Work* work = arg0;
    s32              i;

    for (i = 1; i < 6; i++) {
        work->slots[i].field_9 = work->field_176 + work->field_178;
        Gp_AnimResetSlot(&work->anim, i, work->field_174);
    }
    work->field_172 = work->field_174;
}

/// Advances animation slots 1..5 by one tick.
static __inline__ void Actor204000_TickSlots(Actor104000* arg0)
{
    Actor104000Work* work;
    s32              i;

    work = arg0->field_1C;
    for (i = 1; i < 6; i++) {
        work->slots[i].field_9 = work->field_176 + work->field_178;
        Gp_AnimTickIndex(&work->anim, i);
    }
}

/// Animation step: states 1 and 2 reset the slots and move to 3, which ticks
/// them and counts frames (`field_17A`) and loop ends (`field_17C`).
void func_actor_204000_8014AC8C(Actor104000* arg0)
{
    Actor104000Work* work;

    work = arg0->field_1C;
    if (work->field_170 == 1) {
        Actor204000_ResetSlots(work);
        work->field_170 = 3;
        work->field_17A = 0;
        work->field_17C = 0;
    } else if (work->field_170 == 2) {
        Actor204000_ResetSlots(work);
        work->field_170 = 3;
        work->field_17A = 0;
        work->field_17C = 0;
    } else if (work->field_170 == 3) {
        work->field_17A++;
        Actor204000_TickSlots(arg0);
        if (work->field_58 & 2) {
            work->field_17C++;
        }
    }
}
