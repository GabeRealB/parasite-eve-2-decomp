/* Part of the Sucklerceph library; see sucklerceph.h. */

/// Advances the Sucklerceph's root for one falling frame.
///
/// Requires a live root and `SucklercephWork`. Saves the old translation for
/// collision rollback, moves X/Z along the local matrix's Q12 forward column
/// by signed `forwardSpeed`, and adds `fallSpeed` to Y. Both speeds use parent
/// coordinate units per frame; positive Y falls. The caller applies contacts,
/// accelerates the fall and invalidates composition.
static void _sucklercephFallStep(Task* task)
{
    enum { SUCKLERCEPH_FALL_AXIS_FRACTION_BITS = 12 };

    GfxCoord*        rootCoord;
    SucklercephWork* work;

    rootCoord              = task->extra.tmd->coords;
    work                   = task->work;
    work->prevRootPos.vx   = rootCoord->coord.t[0];
    work->prevRootPos.vy   = rootCoord->coord.t[1];
    work->prevRootPos.vz   = rootCoord->coord.t[2];
    rootCoord->coord.t[0] += (rootCoord->coord.m[0][2] * work->forwardSpeed) >> SUCKLERCEPH_FALL_AXIS_FRACTION_BITS;
    rootCoord->coord.t[1] += work->fallSpeed;
    rootCoord->coord.t[2] += (rootCoord->coord.m[2][2] * work->forwardSpeed) >> SUCKLERCEPH_FALL_AXIS_FRACTION_BITS;
}
