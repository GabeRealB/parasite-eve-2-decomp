/* Part of the factory lift library; see factory_lift.h. */

/// Accelerates the hatch toward closed and sounds its crossing of the shut angle.
///
/// Runs `FACTORY_HATCH_STATE_CLOSE` on a live hatch task with `FactoryHatchWork`
/// and a model root. Angle and angular velocity use 16 fractional bits, with
/// 4096 whole angle units per turn. Rebuilds only the root rotation about X;
/// translation remains intact. The angle may overshoot zero and is retained.
/// Returns 0 through the crossing frame and
/// 1 on subsequent calls, allowing the dispatcher to return to watching.
static s32 _factoryHatchClose(Task* task)
{
    enum {
        FACTORY_HATCH_CLOSE_BEGIN            = 0,
        FACTORY_HATCH_CLOSE_SWING            = 1,
        FACTORY_HATCH_CLOSE_ACCELERATION_Q16 = 0x20000,
        FACTORY_HATCH_CLOSE_MAX_VELOCITY_Q16 = 0x700000
    };
    FactoryHatchWork* work     = task->work;
    GfxCoord*         coord    = task->extra.tmd->coords;
    s32               complete = 0;

    switch (work->step) {
        case FACTORY_HATCH_CLOSE_BEGIN:
            work->angularVelocity = 0;
            work->step++;
            break;
        case FACTORY_HATCH_CLOSE_SWING:
            work->angularVelocity += FACTORY_HATCH_CLOSE_ACCELERATION_Q16;
            if (work->angularVelocity > FACTORY_HATCH_CLOSE_MAX_VELOCITY_Q16) {
                work->angularVelocity = FACTORY_HATCH_CLOSE_MAX_VELOCITY_Q16;
            }
            work->angle.word += work->angularVelocity;
            if (work->angle.word > 0) {
                _factoryHatchPlaySwingSound(coord, SOUND_FACTORY_HATCH_CLOSE, SOUND_NIGHT_FACTORY_HATCH_CLOSE);
                work->step++;
            }
            break;
        default:
            complete = 1;
            break;
    }

    gfxSetRotIdentity(&coord->coord);
    RotMatrixX(work->angle.halves.integer, &coord->coord);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    return complete;
}
