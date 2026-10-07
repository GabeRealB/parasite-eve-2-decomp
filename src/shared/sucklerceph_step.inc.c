/* Part of the Sucklerceph library; see sucklerceph.h. */

/// Advances the Sucklerceph's root for one ground-movement frame.
///
/// Requires a live root and `SucklercephWork`. Saves the old translation for
/// collision rollback, moves X/Z along the local matrix's Q12 forward column
/// by signed `forwardSpeed` game units, and adds 128 to Y for floor correction.
/// The caller resolves contacts and invalidates composition after the step.
static void _sucklercephStep(Task* task)
{
    enum { SUCKLERCEPH_GROUND_FLOOR_STEP  = 128,
           SUCKLERCEPH_AXIS_FRACTION_BITS = 12 };

    GfxCoord*        rootCoord;
    SucklercephWork* work;

    rootCoord              = &task->extra.tmd->coords[0];
    work                   = task->work;
    work->prevRootPos.vx   = rootCoord->coord.t[0];
    work->prevRootPos.vy   = rootCoord->coord.t[1];
    work->prevRootPos.vz   = rootCoord->coord.t[2];
    rootCoord->coord.t[0] += (rootCoord->coord.m[0][2] * work->forwardSpeed) >> SUCKLERCEPH_AXIS_FRACTION_BITS;
    rootCoord->coord.t[2] += (rootCoord->coord.m[2][2] * work->forwardSpeed) >> SUCKLERCEPH_AXIS_FRACTION_BITS;
    rootCoord->coord.t[1] += SUCKLERCEPH_GROUND_FLOOR_STEP;
}
