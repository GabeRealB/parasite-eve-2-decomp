#include "common.h"

#include "actors/actor_105700.h"
#include "gameplay/3A34.h"

/// Hit-reaction state, entry 8 of the `field_6A6` table: step 0 starts
/// animation 0x11 and clears both dwell counters; step 1 waits for frame 0x37,
/// then parks on animation 2 (entry 2) or, with `field_6E0` set, on animation
/// 0x14 (entry 0xA).
void Actor05700_Fn04DA0(Actor105700* arg0)
{
    Actor105700Work* work;
    s16              state;

    work  = arg0->field_1C;
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

/// Entry 9 of the `field_6A6` table: step 0 starts animation 0x12 when
/// `field_6AA` is 1 (step 1, waits for frame 0x50) and animation 0x13
/// otherwise (step 2, waits for frame 0x3B); either way the dwell counters are
/// cleared and the actor parks on animation 2 (entry 2) when done.
void Actor05700_Fn04E2C(Actor105700* arg0)
{
    Actor105700Work* work;
    s32              state;
    s32              next;

    work  = arg0->field_1C;
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

/// Entry 0xA of the `field_6A6` table: step 0 waits for `Gp_TickObjFlag2` on
/// the spawn context to fire, then starts animation 0x13 and clears
/// `field_6E0`; step 1 waits for frame 0x3B and parks on animation 2
/// (entry 2).
void Actor05700_Fn04EF4(Actor105700* arg0)
{
    Actor105700Work* work;
    s16              state;

    work  = arg0->field_1C;
    state = work->field_6A8;
    switch (state) {
        case 0:
            if (Gp_TickObjFlag2((GpObj5D*)arg0->field_20) != 0) {
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

/// Entry 0xE of the `field_6A6` table: step 0 starts animation 0x17 when
/// `field_6B8` is 1 (step 1, waits for frame 0x10) and animation 0x1B
/// otherwise (step 2, waits for frame 0x16); when done the actor's handler
/// chain advances to state 2.
void Actor05700_Fn04F80(Actor105700* arg0)
{
    Actor105700Work* work;
    s32              sel;
    s16              state;

    work  = arg0->field_1C;
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
                arg0->field_30  = 2;
                work->field_6A8 = 0;
            }
            return;
        case 2:
            if (work->field_698 >= 0x16) {
                arg0->field_30  = state;
                work->field_6A8 = 0;
            }
            return;
    }
}
