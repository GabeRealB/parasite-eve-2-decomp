/* Part of the Sucklerceph library; see sucklerceph.h. */

/// Steps the dropping first enemy's root one frame: saves the current
/// translation in `field_274`, advances X and Z along the rotation's Z column
/// scaled by the step length `field_2BE`, and Y by the fall speed `field_2DE`.
void sucklercephFallStep(Task* task)
{
    GfxCoord*        coord;
    Actor104600Work* work;

    coord              = task->extra.tmd->coords;
    work               = (Actor104600Work*)task->work;
    work->field_274.vx = coord->coord.t[0];
    work->field_274.vy = coord->coord.t[1];
    work->field_274.vz = coord->coord.t[2];
    coord->coord.t[0] += (coord->coord.m[0][2] * work->field_2BE) >> 12;
    coord->coord.t[1] += work->field_2DE;
    coord->coord.t[2] += (coord->coord.m[2][2] * work->field_2BE) >> 12;
}
