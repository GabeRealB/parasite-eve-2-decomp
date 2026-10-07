/* Part of the factory lift library; see factory_lift.h. */

/// Plays the hatch swing's stage-bank cue at the model root's audio position.
///
/// Borrows a live coordinate. Both ids are stage-relative scripts; runtime
/// Dryfield selects the day id, and other stages select the night id. Audio pan
/// and depth narrow to signed bytes. This inline definition precedes both swing
/// handlers in each carrier, so the close fragment can use it too.
static inline void _factoryHatchPlaySwingSound(const GfxCoord* coord, s32 daySoundId, s32 nightSoundId)
{
    if (gGameSession->location.loc.stage == GAME_STAGE_DRYFIELD) {
        sndEvtRequestStageScriptStart(daySoundId, (s8)worldCoordGetOriginAudioPan(coord),
                                      (s8)worldCoordGetOriginAudioDepth(coord));
    } else {
        sndEvtRequestStageScriptStart(nightSoundId, (s8)worldCoordGetOriginAudioPan(coord),
                                      (s8)worldCoordGetOriginAudioDepth(coord));
    }
}

/// Sounds and swings the hatch open, overshooting before settling at -768 angle units.
///
/// Runs `FACTORY_HATCH_STATE_OPEN` on a live hatch task with `FactoryHatchWork`
/// and a model root. Angle and angular velocity use 16 fractional bits, with
/// 4096 whole angle units per turn. Rebuilds the root's X rotation while keeping
/// its translation. Returns 0 through the frame that clamps the open angle and
/// 1 on subsequent calls, allowing the dispatcher to return to watching.
static s32 _factoryHatchOpen(Task* task)
{
    enum {
        FACTORY_HATCH_OPEN_BEGIN                   = 0,
        FACTORY_HATCH_OPEN_SWING                   = 1,
        FACTORY_HATCH_OPEN_SETTLE                  = 2,
        FACTORY_HATCH_OPEN_ANGLE_Q16               = -0x3000000,
        FACTORY_HATCH_OPEN_ACCELERATION_Q16        = -0x28000,
        FACTORY_HATCH_OPEN_MIN_VELOCITY_Q16        = -0x300000,
        FACTORY_HATCH_OPEN_SETTLE_ACCELERATION_Q16 = 0x40000,
        FACTORY_HATCH_OPEN_MAX_SETTLE_VELOCITY_Q16 = 0x100000
    };
    FactoryHatchWork* work     = task->work;
    GfxCoord*         coord    = task->extra.tmd->coords;
    s32               complete = 0;

    switch (work->step) {
        case FACTORY_HATCH_OPEN_BEGIN:
            work->angularVelocity = 0;
            _factoryHatchPlaySwingSound(coord, SOUND_FACTORY_HATCH_OPEN, SOUND_NIGHT_FACTORY_HATCH_OPEN);
            work->step++;
            break;
        case FACTORY_HATCH_OPEN_SWING:
            work->angularVelocity += FACTORY_HATCH_OPEN_ACCELERATION_Q16;
            if (work->angularVelocity < FACTORY_HATCH_OPEN_MIN_VELOCITY_Q16) {
                work->angularVelocity = FACTORY_HATCH_OPEN_MIN_VELOCITY_Q16;
            }
            work->angle.word += work->angularVelocity;
            if (work->angle.word < FACTORY_HATCH_OPEN_ANGLE_Q16) {
                work->step++;
            }
            break;
        case FACTORY_HATCH_OPEN_SETTLE:
            work->angularVelocity += FACTORY_HATCH_OPEN_SETTLE_ACCELERATION_Q16;
            if (work->angularVelocity > FACTORY_HATCH_OPEN_MAX_SETTLE_VELOCITY_Q16) {
                work->angularVelocity = FACTORY_HATCH_OPEN_MAX_SETTLE_VELOCITY_Q16;
            }
            work->angle.word += work->angularVelocity;
            if (work->angle.word >= FACTORY_HATCH_OPEN_ANGLE_Q16) {
                work->angle.word = FACTORY_HATCH_OPEN_ANGLE_Q16;
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
