/* Part of the Moth library; see moth.h. */

/// Flaps the model's two wing parts in opposite directions about local Z.
///
/// Requires the moth work and four-part model. Every 16 living ticks rerolls
/// slow/fast mode. Slow mode sweeps -256, 0, +256 angular units and reverses
/// the swing sign on wrapping; fast mode alternates the full swing each tick.
/// Parts 2 and 3 retain their translations and have composition marked dirty.
/// The scratch angle vector is released before return; 4096 units is one turn.
static void _mothOscillateParts(Task* task)
{
    enum {
        MOTH_FLAP_MODE_TICKS = 16,
        MOTH_FLAP_ANGLE      = 0x100,
        MOTH_FLAP_SWEEP_END  = 2 * MOTH_FLAP_ANGLE,
        MOTH_LEFT_WING_PART  = 2,
        MOTH_RIGHT_WING_PART = 3
    };

    MothWork* work;
    GfxCoord* leftWingCoords;
    GfxCoord* rightWingCoords;
    SVECTOR*  wingAngles;
    s32       slowFlapSign;
    s32       fastFlapSign;
    s32       wingAngle;

    wingAngles = SCRATCH_STACK_RESERVE_BLOCK(SVECTOR);
    work       = task->work;
    if (++work->timer >= MOTH_FLAP_MODE_TICKS) {
        work->timer     = 0;
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        work->flapFast  = !((gRandomLcgState >> 16) & 1);
    }
    switch (work->flapFast) {
        case false:
            work->flapAngle += MOTH_FLAP_ANGLE;
            if (work->flapAngle >= MOTH_FLAP_SWEEP_END) {
                work->flapAngle = -MOTH_FLAP_ANGLE;
                slowFlapSign    = work->flapSign;
                work->flapSign  = -slowFlapSign;
            }
            break;
        case true:
            work->flapAngle = MOTH_FLAP_ANGLE;
            fastFlapSign    = work->flapSign;
            work->flapSign  = -fastFlapSign;
            break;
    }
    wingAngles->vx = 0;
    wingAngles->vy = 0;
    wingAngles->vz = work->flapAngle * work->flapSign;
    leftWingCoords = task->extra.tmd->coords;
    RotMatrix(wingAngles, &leftWingCoords[MOTH_LEFT_WING_PART].coord);
    leftWingCoords[MOTH_LEFT_WING_PART].composeStamp = GRAPHICS_COORD_DIRTY;
    wingAngles->vx                                   = 0;
    wingAngles->vy                                   = 0;
    wingAngle                                        = work->flapAngle * work->flapSign;
    wingAngles->vz                                   = -wingAngle;
    rightWingCoords                                  = task->extra.tmd->coords;
    RotMatrix(wingAngles, &rightWingCoords[MOTH_RIGHT_WING_PART].coord);
    rightWingCoords[MOTH_RIGHT_WING_PART].composeStamp = GRAPHICS_COORD_DIRTY;
    SCRATCH_STACK_RELEASE_BLOCK(SVECTOR);
}
