/* Part of the Knight and Bishop GOLEM library; see golem_knight_bishop.h. */

/// Saves the root's translation in `field_664`..`field_66C` and steps it
/// `field_6C8` along the root's facing (its matrix's third column), adding
/// 0x80 to its y while `field_714` is below 2.
void golemKnightBishopStepForward(Task* arg0)
{
    GolemKnightBishopWork* work;
    GfxCoord*              coord;

    coord              = &arg0->extra.tmd->coords[0];
    work               = arg0->work;
    work->field_664    = coord->coord.t[0];
    work->field_668    = coord->coord.t[1];
    work->field_66C    = coord->coord.t[2];
    coord->coord.t[0] += (coord->coord.m[0][2] * work->field_6C8) >> 12;
    if (work->field_714 < 2) {
        coord->coord.t[1] += 0x80;
    }
    coord->coord.t[2] += (coord->coord.m[2][2] * work->field_6C8) >> 12;
}
