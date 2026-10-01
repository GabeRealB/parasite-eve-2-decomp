/* Part of the Rat library; see rat.h. */

/// Records the model's current root position in the work block, then displaces
/// the root coordinate by the work's step along the rotation's third column
/// (X and Z only) and by 0x80 on Y.
void ratStep(Task* arg0)
{
    RatWork*  work;
    GfxCoord* coord;

    coord              = arg0->extra.tmd->coords;
    work               = arg0->work;
    work->field_360    = coord->coord.t[0];
    work->field_364    = coord->coord.t[1];
    work->field_368    = coord->coord.t[2];
    coord->coord.t[0] += (s32)(coord->coord.m[0][2] * work->field_384) >> 0xC;
    coord->coord.t[1] += 0x80;
    coord->coord.t[2] += (s32)(coord->coord.m[2][2] * work->field_384) >> 0xC;
}
