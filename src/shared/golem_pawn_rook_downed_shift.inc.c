#include "main/random.h"

/* Part of the Pawn and Rook GOLEM library; see golem_pawn_rook.h. */

/// Entry 0xC of the `behavior` table. State 0 picks the animation from
/// `downedPose`: 1 selects 0x17 into state 1, anything else 0x1B into state 2.
/// States 1 and 2 wait for `animFrame` to reach 0x10 / 0x16, then settle on
/// the matching idle (0x19 / 0x1D), move to entry 0xB's state 3 and roll a
/// fresh 6-bit dwell into `timer`.
void golemPawnRookDownedShiftState(Task* arg0)
{
    GolemPawnRookWork* work;
    s16                state;
    s32                next;

    work  = arg0->work;
    state = work->step;
    switch (state) {
        case 0:
            next = work->downedPose;
            if (next == 1) {
                work->anim = 0x17;
                work->step = next;
            } else {
                work->anim = 0x1B;
                work->step = 2;
            }
            break;
        case 1:
            if (work->animFrame >= 0x10) {
                work->anim      = 0x19;
                work->behavior  = GOLEM_PAWN_ROOK_BEHAVIOR_KNOCKDOWN;
                work->step      = 3;
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->timer     = (gRandomLcgState >> 16) & 0x3F;
            }
            break;
        case 2:
            if (work->animFrame >= 0x16) {
                work->anim      = 0x1D;
                work->behavior  = GOLEM_PAWN_ROOK_BEHAVIOR_KNOCKDOWN;
                work->step      = 3;
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->timer     = (gRandomLcgState >> 16) & 0x3F;
            }
            break;
    }
}
