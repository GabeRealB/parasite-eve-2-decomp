/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Saves the root Y as the ground height in `field_92`, requests animation 8
/// at speed 0x10, clears the frame counter and the motion halfwords, sets
/// `field_440` and advances the sub-state.
void madChaserStartLeap(Task* arg0)
{
    MadChaserWork* work;
    MadChaserWork* work2;
    s16            tmp;

    work             = (MadChaserWork*)arg0->work;
    work->field_92   = (u16)arg0->extra.tmd->coords->coord.t[1];
    work2            = (MadChaserWork*)arg0->work;
    tmp              = 8;
    work2->field_426 = tmp;
    work2->field_418 = tmp;
    work2->field_41C = 0x10;
    tmp              = 1;
    work2->field_414 = tmp;
    work->field_412  = 0;
    work->field_428  = 0;
    work->field_42A  = -0x12C;
    work->field_440  = tmp;
    work->field_438  = 0;
    work->field_432  = 0;
    work->field_422  = work->field_422 + 1;
}
