#include "common.h"
#include "actors/actor_104000.h"

/// Seeds animation slots 1..5 with the requested animation id and blend speed.
static __inline__ void Actor204000_ResetSlots(Actor104000Work* arg0)
{
    Actor104000Work* work = arg0;
    s32              i;

    for (i = 1; i < 6; i++) {
        work->slots[i].rate = work->field_176 + work->field_178;
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
        work->slots[i].rate = work->field_176 + work->field_178;
        Gp_AnimTickIndex(&work->anim, i);
    }
}

/// Steps the motion state in `field_170`: states 1 and 2 restart slots 1..5 on
/// the animation in `field_174`, clear the frame and loop counters and move to
/// state 3; state 3 ticks the slots, counting frames in `field_17A` and, while
/// bit 2 of `field_58` is set, loops in `field_17C`.
void Actor04000_Fn00E6C(Actor104000* arg0)
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
