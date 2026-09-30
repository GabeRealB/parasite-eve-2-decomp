#include "main/random.h"

/* Part of the hopping enemy library; see hopping_enemy.h. */

/// Requests animation 1, draws a 0x60..0x9F frame hold into `field_446`,
/// clears the frame counter and advances the sub-state.
void hopperStartHold(Task* arg0)
{
    Actor341700Work* work;

    work            = (Actor341700Work*)arg0->work;
    work->field_426 = 4;
    work->field_41C = 0x10;
    work->field_418 = 1;
    work->field_414 = 1;
    /* Rolling the LCG through the global rather than an m2c temporary is what
     * hoists its `lw` above the field stores; see DECOMPILATION_LEARNINGS.md. */
    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    work->field_446 = ((gRandomLcgState >> 16) & 0x3F) + 0x60;
    work->field_412 = 0;
    work->field_422 = work->field_422 + 1;
}
