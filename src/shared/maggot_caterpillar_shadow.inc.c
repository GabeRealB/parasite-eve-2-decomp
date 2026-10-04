/* Part of the Maggot and Caterpillar library; see maggot_caterpillar.h. */

/// Ground shadow under the actor's coordinate. In `MAGGOT_CATERPILLAR_BEHAVIOUR_AMBUSH` the
/// position is cast down to the ground by `func_800EA1A8` and the shade comes
/// from `func_800EA318`; otherwise the coordinate's own world translation is
/// used at full shade.
void maggotCaterpillarDrawShadow(Task* arg0)
{
    MaggotCaterpillarWork* work;
    GfxCoord*              coord;
    VECTOR3                vec;
    s16                    hit;

    work  = arg0->work;
    coord = arg0->extra.tmd->coords;
    if (work->behaviour == MAGGOT_CATERPILLAR_BEHAVIOUR_AMBUSH) {
        hit = func_800EA1A8(MATRIX_TRANS(&coord->workm), &vec);
        if (hit != 0) {
            Gp_DrawEffGroundQuad(&vec, 0x200, func_800EA318(0x200, 0x80, hit));
        }
    } else {
        vec.vx = coord->workm.t[0];
        vec.vy = coord->workm.t[1];
        vec.vz = coord->workm.t[2];
        Gp_DrawEffGroundQuad(&vec, 0x200, 0x80);
    }
}
