#include "common.h"

#include "gameplay/1BC.h"

#include "actors/actor_450800.h"

/// Ticks the actor's animation slots 1..0x13.
void func_actor_450800_80132A1C(Task* task)
{
    Actor450800Work* work;
    s32              i;

    work = (Actor450800Work*)task->work;
    i    = 1;
    do {
        Gp_AnimTickIndex(&work->anim, i);
        i++;
    } while (i < 0x14);
}

/// Resets the actor's animation slots 1..0x13 to clip `field_4B8` at rate 1,
/// without a reset argument, and latches the clip into `field_4B6`.
void func_actor_450800_80132A68(Task* task)
{
    Actor450800Work* work;
    s32              i;

    work = (Actor450800Work*)task->work;
    i    = 1;
    do {
        work->slots[i].rate = 1;
        Gp_AnimResetSlot(&work->anim, i, work->field_4B8);
        i++;
    } while (i < 0x14);
    work->field_4B6 = work->field_4B8;
}
