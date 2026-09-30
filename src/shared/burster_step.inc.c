/* Part of the burster library; see burster.h. */

/// Steps the first enemy's root one frame along its own facing: saves the
/// current translation in `field_274`, advances X and Z along the rotation's Z
/// column scaled by the step length `field_2BE`, and Y by a fixed 0x80.
void bursterStep(Task* task)
{
    GfxCoord*        coord;
    Actor104600Work* work;

    coord              = &task->extra.tmd->coords[0];
    work               = (Actor104600Work*)task->work;
    work->field_274.vx = coord->coord.t[0];
    work->field_274.vy = coord->coord.t[1];
    work->field_274.vz = coord->coord.t[2];
    coord->coord.t[0] += (coord->coord.m[0][2] * work->field_2BE) >> 12;
    coord->coord.t[2] += (coord->coord.m[2][2] * work->field_2BE) >> 12;
    coord->coord.t[1] += 0x80;
}
