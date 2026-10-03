/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Saves the root Y as the ground height in `moveStartPos.vy`, requests animation 8
/// at speed 0x10, clears the frame counter and the jump motion, sets
/// `hasLeaped` and advances the sub-state.
void madChaserStartLeap(Task* arg0)
{
    MadChaserWork* work;
    MadChaserWork* work2;
    s16            tmp;

    work                   = (MadChaserWork*)arg0->work;
    work->moveStartPos.vy  = arg0->extra.tmd->coords->coord.t[1];
    work2                  = (MadChaserWork*)arg0->work;
    tmp                    = 8;
    work2->animBlendFrames = tmp;
    work2->animId          = tmp;
    work2->animRate        = ANIMATION_RATE_ONE;
    tmp                    = 1;
    work2->animRequest     = tmp;
    work->stateFrames      = 0;
    work->moveAccel        = 0;
    work->moveSpeed        = -0x12C;
    work->hasLeaped        = tmp;
    work->busy             = 0;
    work->anchored         = 0;
    work->subState         = work->subState + 1;
}
