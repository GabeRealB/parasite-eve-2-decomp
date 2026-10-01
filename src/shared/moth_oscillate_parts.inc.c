/* Part of the Moth library; see moth.h. */

/// Sweeps the actor's spare rotation on the scratchpad: every 16th frame rolls
/// `gRandomLcgState` to pick a direction, then `field_2D8` ramps between `-0x100`
/// and `0x100` and flips the `field_2D6` sign each time it wraps. The ramped
/// value scaled by that sign is the pitch written into the scratch vector,
/// which is handed to `RotMatrix` twice - once against `coord[2]`, once with
/// the product negated against `coord[3]`.
void mothOscillateParts(Task* arg0)
{
    MothWork* work;
    GfxCoord* coord;
    GfxCoord* coord2;
    SVECTOR*  sc;
    s32       direction;
    s32       direction2;
    s32       product;

    sc   = (SVECTOR*)SCRATCH_STACK_RESERVE_BYTES(8);
    work = arg0->work;
    if (++work->field_2E0 >= 16) {
        work->field_2E0 = 0;
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        work->field_2D4 = !((gRandomLcgState >> 16) & 1);
    }
    switch (work->field_2D4) {
        case 0:
            work->field_2D8 += 0x100;
            if (work->field_2D8 >= 0x200) {
                work->field_2D8 = -0x100;
                direction       = work->field_2D6;
                work->field_2D6 = -direction;
            }
            break;
        case 1:
            work->field_2D8 = 0x100;
            direction2      = work->field_2D6;
            work->field_2D6 = -direction2;
            break;
    }
    sc->vx = 0;
    sc->vy = 0;
    sc->vz = work->field_2D8 * work->field_2D6;
    coord  = arg0->extra.tmd->coords;
    RotMatrix(sc, &coord[2].coord);
    coord[2].composeStamp = GRAPHICS_COORD_DIRTY;
    sc->vx                = 0;
    sc->vy                = 0;
    product               = work->field_2D8 * work->field_2D6;
    sc->vz                = -product;
    coord2                = arg0->extra.tmd->coords;
    RotMatrix(sc, &coord2[3].coord);
    coord2[3].composeStamp = GRAPHICS_COORD_DIRTY;
    SCRATCH_STACK_RELEASE_BYTES(8);
}
