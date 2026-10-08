/* Part of the Maggot and Caterpillar library; see maggot_caterpillar.h. */

/// Draws the root's ground shadow, probing the floor during a hanging ambush.
///
/// Requires initialized work and a composed root coordinate. The shadow has
/// a 512-unit half-size and base grayscale 128. During the ambush, the probe's
/// signed view-Y displacement narrows to s16 before the hit test and shade
/// calculation; a zero result draws nothing. Other behaviors use the cached
/// root translation directly, without refreshing its composition.
static void _maggotCaterpillarDrawShadow(Task* actor)
{
    enum {
        MAGGOT_CATERPILLAR_SHADOW_HALF_SIZE = 512,
        MAGGOT_CATERPILLAR_SHADOW_SHADE     = 128
    };
    MaggotCaterpillarWork* work;
    GfxCoord*              coord;
    VECTOR3                shadowPosition;
    s16                    probeDisplacement;

    work  = actor->work;
    coord = actor->extra.tmd->coords;
    if (work->behaviour == MAGGOT_CATERPILLAR_BEHAVIOUR_AMBUSH) {
        probeDisplacement = worldCollisionProjectGroundPoint(MATRIX_TRANS(&coord->workm), &shadowPosition);
        if (probeDisplacement != 0) {
            effectDrawGroundShadow(&shadowPosition, MAGGOT_CATERPILLAR_SHADOW_HALF_SIZE, effectGetGroundShadowShade(MAGGOT_CATERPILLAR_SHADOW_HALF_SIZE, MAGGOT_CATERPILLAR_SHADOW_SHADE, probeDisplacement));
        }
    } else {
        shadowPosition.vx = coord->workm.t[0];
        shadowPosition.vy = coord->workm.t[1];
        shadowPosition.vz = coord->workm.t[2];
        effectDrawGroundShadow(&shadowPosition, MAGGOT_CATERPILLAR_SHADOW_HALF_SIZE, MAGGOT_CATERPILLAR_SHADOW_SHADE);
    }
}
