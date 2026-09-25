#include "common.h"
#include "actors/actor_201200.h"
#include "gameplay/1BC.h"

/// Restarts animation slots 1 to 5 on the requested animation `field_174`, at
/// the rate `field_176 + field_178`, and records it as the running one.
static __inline__ void Actor01200_ResetSlots(Actor201200Work* arg0)
{
    Actor201200Work* work = arg0;
    s32              i;

    for (i = 1; i < 6; i++) {
        work->slots[i].rate = work->field_176 + work->field_178;
        Gp_AnimResetSlot(&work->anim, i, work->field_174);
    }
    work->field_172 = work->field_174;
}

/// Advances animation slots 1 to 5 by one frame at the rate
/// `field_176 + field_178`.
static __inline__ void Actor01200_TickSlots(Actor201200* arg0)
{
    Actor201200Work* work;
    s32              i;

    work = arg0->field_1C;
    for (i = 1; i < 6; i++) {
        work->slots[i].rate = work->field_176 + work->field_178;
        Gp_AnimTickIndex(&work->anim, i);
    }
}

/// Motion driver the state handlers run every frame. A start request in
/// `field_170` (1 or 2) restarts the animation slots and clears the frame
/// counters `field_17A` / `field_17C`; while running (3) it advances the
/// slots, counts frames in `field_17A` and, while bit 1 of `field_58` is set,
/// in `field_17C` as well.
void Actor01200_Fn00820(Actor201200* arg0)
{
    Actor201200Work* work;

    work = arg0->field_1C;
    if (work->field_170 == 1) {
        Actor01200_ResetSlots(work);
        work->field_170 = 3;
        work->field_17A = 0;
        work->field_17C = 0;
    } else if (work->field_170 == 2) {
        Actor01200_ResetSlots(work);
        work->field_170 = 3;
        work->field_17A = 0;
        work->field_17C = 0;
    } else if (work->field_170 == 3) {
        work->field_17A++;
        Actor01200_TickSlots(arg0);
        if (work->field_58 & 2) {
            work->field_17C++;
        }
    }
}
