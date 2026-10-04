/* Part of the Knight and Bishop GOLEM library; see golem_knight_bishop.h. */

/// Saves the root's translation in `prevRootPos` and steps it `forwardSpeed`
/// along the root's facing (its matrix's third column), adding 0x80 to its y
/// while `knockdownStage` is below 2.
void golemKnightBishopStepForward(Task* arg0)
{
    GolemKnightBishopWork* work;
    GfxCoord*              coord;

    coord                = &arg0->extra.tmd->coords[0];
    work                 = arg0->work;
    work->prevRootPos.vx = coord->coord.t[0];
    work->prevRootPos.vy = coord->coord.t[1];
    work->prevRootPos.vz = coord->coord.t[2];
    coord->coord.t[0]   += (coord->coord.m[0][2] * work->forwardSpeed) >> 12;
    if (work->knockdownStage < 2) {
        coord->coord.t[1] += 0x80;
    }
    coord->coord.t[2] += (coord->coord.m[2][2] * work->forwardSpeed) >> 12;
}
