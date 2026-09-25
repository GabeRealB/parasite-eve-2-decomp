#include "common.h"

#include "actors/actor_105600.h"
#include "gameplay/3A34.h"
#include "main/task.h"

/// Hit-reaction state, entry 8 of `Actor05600_D16540`: step 0 starts
/// animation 0x11 and stops the actor; step 1 waits for frame 0x37, then parks
/// on animation 2 (entry 2) or, with `field_6E0` set, on animation 0x14
/// (entry 0xA).
void Actor05600_Fn047D0(Task* task)
{
    Actor105600Work* work;
    s16              state;

    work  = (Actor105600Work*)task->work;
    state = work->field_6A8;
    switch (state) {
        case 0:
            work->field_694 = 0x11;
            work->field_6A8 = 1;
            work->field_69C = 0;
            work->field_69E = 0;
            break;
        case 1:
            if (work->field_698 >= 0x37) {
                if (work->field_6E0 == 0) {
                    work->field_694 = 2;
                    work->field_6A6 = 2;
                    work->field_6A8 = 0;
                } else {
                    work->field_694 = 0x14;
                    work->field_6A6 = 0xA;
                    work->field_6A8 = 0;
                }
            }
            break;
    }
}

/// Entry 9 of `Actor05600_D16540`: step 0 starts animation 0x12 when
/// `field_6AA` is 1 (step 1, waits for frame 0x50) and animation 0x13
/// otherwise (step 2, waits for frame 0x3B); either way the actor stops, and
/// it parks on animation 2 (entry 2) when done.
void Actor05600_Fn0485C(Task* task)
{
    Actor105600Work* work;
    s32              state;
    s32              next;

    work  = (Actor105600Work*)task->work;
    state = work->field_6A8;
    switch (state) {
        case 0:
            next = work->field_6AA;
            if (next == 1) {
                work->field_694 = 0x12;
                work->field_6A8 = next;
            } else {
                work->field_694 = 0x13;
                work->field_6A8 = 2;
            }
            work->field_69C = 0;
            work->field_69E = 0;
            break;
        case 1:
            if (work->field_698 >= 0x50) {
                work->field_694 = 2;
                work->field_6A6 = 2;
                work->field_6A8 = 0;
            }
            break;
        case 2:
            if (work->field_698 >= 0x3B) {
                work->field_694 = state;
                work->field_6A6 = state;
                work->field_6A8 = 0;
            }
            break;
    }
}

/// Entry 0xA of `Actor05600_D16540`: step 0 waits for `Gp_TickObjFlag2` on
/// the spawn context to fire, then starts animation 0x13 and clears
/// `field_6E0`; step 1 waits for frame 0x3B and parks on animation 2
/// (entry 2).
void Actor05600_Fn04924(Task* task)
{
    Actor105600Work* work;
    s16              state;

    work  = (Actor105600Work*)task->work;
    state = work->field_6A8;
    switch (state) {
        case 0:
            if (Gp_TickObjFlag2((GpObj5D*)task->spawnArg2) != 0) {
                work->field_694 = 0x13;
                work->field_6A8 = 1;
                work->field_6E0 = 0;
            }
            break;
        case 1:
            if (work->field_698 >= 0x3B) {
                work->field_694 = 2;
                work->field_6A6 = 2;
                work->field_6A8 = 0;
            }
            break;
    }
}

/// Entry 0xE of `Actor05600_D16540`: step 0 starts animation 0x17 when
/// `field_6B8` is 1 (step 1, waits for frame 0x10) and animation 0x1B
/// otherwise (step 2, waits for frame 0x16); when done the task advances to
/// state 2.
void Actor05600_Fn049B0(Task* task)
{
    Actor105600Work* work;
    s32              sel;
    s16              state;

    work  = (Actor105600Work*)task->work;
    state = work->field_6A8;
    switch (state) {
        case 0:
            /* The 32-bit local is load-bearing: an s16 one makes combine fold
             * the sign-extension into a second `lh` of field_6B8. */
            sel = work->field_6B8;
            if (sel == 1) {
                work->field_694 = 0x17;
                work->field_6A8 = sel;
                return;
            }
            work->field_694 = 0x1B;
            work->field_6A8 = 2;
            return;
        case 1:
            if (work->field_698 >= 0x10) {
                task->state     = 2;
                work->field_6A8 = 0;
            }
            return;
        case 2:
            if (work->field_698 >= 0x16) {
                task->state     = state;
                work->field_6A8 = 0;
            }
            return;
    }
}
