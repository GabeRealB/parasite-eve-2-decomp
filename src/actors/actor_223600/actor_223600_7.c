#include "common.h"

#include "actors/actor_223600.h"
#include "gameplay/1BC.h"
#include "main/task.h"

/// Restarts animation slots 1-5 on the motion in `field_174` at the combined
/// rate `field_176 + field_178`, and records that motion as the one playing.
static __inline__ void Actor223600_ResetSlots(Actor223600Work* arg0)
{
    Actor223600Work* work = arg0;
    s32              i;

    for (i = 1; i < 6; i++) {
        work->slots[i].rate = work->field_176 + work->field_178;
        Gp_AnimResetSlot(&work->anim, i, work->field_174);
    }
    work->field_172 = work->field_174;
}

/// Advances animation slots 1-5 by one frame at the combined rate
/// `field_176 + field_178`.
static __inline__ void Actor223600_TickSlots(Task* task)
{
    Actor223600Work* work;
    s32              i;

    work = task->work;
    for (i = 1; i < 6; i++) {
        work->slots[i].rate = work->field_176 + work->field_178;
        Gp_AnimTickIndex(&work->anim, i);
    }
}

/// Drives the model's motion from the work block's `field_170`: 1 and 2
/// restart the slots on `field_174` and move to 3, clearing both frame
/// counters; 3 advances the slots one frame, counting it in `field_17A` and,
/// while `field_58` bit 1 is set, in `field_17C` too.
void func_actor_223600_8014B2F4(Task* task)
{
    Actor223600Work* work;

    work = task->work;
    if (work->field_170 == 1) {
        Actor223600_ResetSlots(work);
        work->field_170 = 3;
        work->field_17A = 0;
        work->field_17C = 0;
    } else if (work->field_170 == 2) {
        Actor223600_ResetSlots(work);
        work->field_170 = 3;
        work->field_17A = 0;
        work->field_17C = 0;
    } else if (work->field_170 == 3) {
        work->field_17A++;
        Actor223600_TickSlots(task);
        if (work->field_58 & 2) {
            work->field_17C++;
        }
    }
}
