/* Part of the Sucklerceph library; see sucklerceph.h. */

/// Steps the first enemy's root one frame along its own facing: saves the
/// current translation in `prevRootPos`, advances X and Z along the rotation's Z
/// column scaled by the step length `forwardSpeed`, and Y by a fixed 0x80.
void sucklercephStep(Task* task)
{
    GfxCoord*        coord;
    SucklercephWork* work;

    coord                = &task->extra.tmd->coords[0];
    work                 = task->work;
    work->prevRootPos.vx = coord->coord.t[0];
    work->prevRootPos.vy = coord->coord.t[1];
    work->prevRootPos.vz = coord->coord.t[2];
    coord->coord.t[0]   += (coord->coord.m[0][2] * work->forwardSpeed) >> 12;
    coord->coord.t[2]   += (coord->coord.m[2][2] * work->forwardSpeed) >> 12;
    coord->coord.t[1]   += 0x80;
}
