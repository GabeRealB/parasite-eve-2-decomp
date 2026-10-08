/* Part of the No. 9 GOLEM library; see no9_golem.h. */

/// Draws the No. 9 GOLEM's ground shadow below its body coordinate.
///
/// Requires a live model with root and body coordinates at indices 0 and 1,
/// already composed into the same frame. Uses the body's X/Z and the root's
/// Y without refreshing either cache. The square has a 768-unit half-side
/// and grayscale shade 128; the effect drawer controls visibility and packets.
static void _no9GolemDrawShadow(const Task* task)
{
    enum { NO9_GOLEM_SHADOW_HALF_SIZE = 768,
           NO9_GOLEM_SHADOW_SHADE     = 128 };
    const GfxCoord* rootCoord;
    const GfxCoord* bodyCoord;
    VECTOR3         shadowCentre;

    rootCoord       = task->extra.tmd->coords;
    bodyCoord       = &task->extra.tmd->coords[1];
    shadowCentre.vx = bodyCoord->workm.t[0];
    shadowCentre.vy = rootCoord->workm.t[1];
    shadowCentre.vz = bodyCoord->workm.t[2];
    effectDrawGroundShadow(&shadowCentre, NO9_GOLEM_SHADOW_HALF_SIZE, NO9_GOLEM_SHADOW_SHADE);
}
