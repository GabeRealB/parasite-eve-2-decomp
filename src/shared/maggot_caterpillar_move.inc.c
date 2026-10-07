/* Part of the Maggot and Caterpillar library; see maggot_caterpillar.h. */

/// Saves the root translation and applies the current horizontal and falling steps.
///
/// `forwardSpeed` and `fallSpeed` are coordinate units per tick. Horizontal
/// movement uses the root forward-axis coefficients with 12 fractional bits;
/// fall speed is added directly to Y. The caller refreshes the composed matrix
/// after behavior, turning, movement and animation finish.
static void _maggotCaterpillarMoveStep(Task* actor)
{
    GfxCoord*              coord;
    MaggotCaterpillarWork* work;

    coord = actor->extra.tmd->coords;
    work  = actor->work;

    work->prevPos.vx = coord->coord.t[0];
    work->prevPos.vy = coord->coord.t[1];
    work->prevPos.vz = coord->coord.t[2];

    coord->coord.t[0] += (coord->coord.m[0][2] * work->forwardSpeed) >> 12;
    coord->coord.t[1] += work->fallSpeed;
    coord->coord.t[2] += (coord->coord.m[2][2] * work->forwardSpeed) >> 12;
}
