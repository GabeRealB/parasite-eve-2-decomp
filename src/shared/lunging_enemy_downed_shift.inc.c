#include "main/random.h"

/* Part of the lunging enemy library; see lunging_enemy.h. */

/// Entry 0xC of the `field_6A6` table. State 0 picks the animation from
/// `field_6B8`: 1 selects 0x17 into state 1, anything else 0x1B into state 2.
/// States 1 and 2 wait for `field_698` to reach 0x10 / 0x16, then settle on
/// the matching idle (0x19 / 0x1D), move to entry 0xB's state 3 and roll a
/// fresh 6-bit dwell into `field_6AE`.
void lungerDownedShiftState(Task* arg0)
{
    Actor105600Work* work;
    s16              state;
    s32              next;

    work  = arg0->work;
    state = work->field_6A8;
    switch (state) {
        case 0:
            next = work->field_6B8;
            if (next == 1) {
                work->field_694 = 0x17;
                work->field_6A8 = next;
            } else {
                work->field_694 = 0x1B;
                work->field_6A8 = 2;
            }
            break;
        case 1:
            if (work->field_698 >= 0x10) {
                work->field_694 = 0x19;
                work->field_6A6 = 0xB;
                work->field_6A8 = 3;
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->field_6AE = (gRandomLcgState >> 16) & 0x3F;
            }
            break;
        case 2:
            if (work->field_698 >= 0x16) {
                work->field_694 = 0x1D;
                work->field_6A6 = 0xB;
                work->field_6A8 = 3;
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->field_6AE = (gRandomLcgState >> 16) & 0x3F;
            }
            break;
    }
}
