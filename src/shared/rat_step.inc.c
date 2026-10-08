/* Part of the Rat library; see rat.h. */

/// Saves the root position and applies this frame's horizontal and vertical step.
///
/// Requires live work/model. Forward speed is in parent-coordinate units per
/// update; the root's third rotation column has 12 fractional bits. X/Z use
/// arithmetic right shifts, and local Y increases by 128. Grid-contact recovery
/// can restore the saved position on the following contact pass; the caller
/// owns coordinate invalidation and composition.
static void _ratStep(Task* actor)
{
    enum {
        RAT_STEP_VERTICAL_SPEED = 128,
    };

    RatWork*  work;
    GfxCoord* rootCoord;

    rootCoord              = actor->extra.tmd->coords;
    work                   = actor->work;
    work->prevPos.vx       = rootCoord->coord.t[0];
    work->prevPos.vy       = rootCoord->coord.t[1];
    work->prevPos.vz       = rootCoord->coord.t[2];
    rootCoord->coord.t[0] += (rootCoord->coord.m[0][2] * work->forwardSpeed) >> RAT_DIRECTION_FRACTION_BITS;
    rootCoord->coord.t[1] += RAT_STEP_VERTICAL_SPEED;
    rootCoord->coord.t[2] += (rootCoord->coord.m[2][2] * work->forwardSpeed) >> RAT_DIRECTION_FRACTION_BITS;
}
