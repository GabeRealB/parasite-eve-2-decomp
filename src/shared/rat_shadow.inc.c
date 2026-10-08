/* Part of the Rat library; see rat.h. */

/// Draws the rat's ground shadow from its cached root translation.
///
/// Requires a live model with a cache suitable for the ground-shadow query.
/// The square has a half-side of 448 coordinate units and shade 128; this
/// function does not refresh the coordinate cache.
static void _ratShadow(Task* actor)
{
    enum {
        RAT_SHADOW_HALF_SIZE = 448,
        RAT_SHADOW_SHADE     = 128,
    };

    GfxCoord* rootCoord;
    VECTOR3   shadowCentre;

    rootCoord       = actor->extra.tmd->coords;
    shadowCentre.vx = rootCoord->workm.t[0];
    shadowCentre.vy = rootCoord->workm.t[1];
    shadowCentre.vz = rootCoord->workm.t[2];
    effectDrawGroundShadow(&shadowCentre, RAT_SHADOW_HALF_SIZE, RAT_SHADOW_SHADE);
}
