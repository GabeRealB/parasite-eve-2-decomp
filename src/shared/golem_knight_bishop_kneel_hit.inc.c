#include "main/random.h"

/* Part of the Knight and Bishop GOLEM library; see golem_knight_bishop.h. */

/// A hit taken on the ground: step 0 takes animation 0xE (and step 1) when
/// `downedPose` is 1, otherwise 0x12 and step 2; steps 1 and 2 wait for
/// `animFrame` to reach 0x10 or 0x16, then take the lying animation 0x10 or
/// 0x14 and re-enter the kneel sequence at step 3 with `timer` rolled from
/// the `gRandomLcgState` LCG (0..0x3F).
void golemKnightBishopKneelHitSeq(Task* arg0)
{
    GolemKnightBishopWork* work;
    s16                    state;
    s32                    next;

    work  = arg0->work;
    state = work->step;
    switch (state) {
        case 0:
            next = work->downedPose;
            if (next == 1) {
                work->anim = 0xE;
                work->step = next;
            } else {
                work->anim = 0x12;
                work->step = 2;
            }
            break;
        case 1:
            if (work->animFrame >= 0x10) {
                work->anim      = 0x10;
                work->sequence  = GOLEM_KNIGHT_BISHOP_SEQUENCE_KNEEL;
                work->step      = 3;
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->timer     = (gRandomLcgState >> 16) & 0x3F;
            }
            break;
        case 2:
            if (work->animFrame >= 0x16) {
                work->anim      = 0x14;
                work->sequence  = GOLEM_KNIGHT_BISHOP_SEQUENCE_KNEEL;
                work->step      = 3;
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                work->timer     = (gRandomLcgState >> 16) & 0x3F;
            }
            break;
    }
}
