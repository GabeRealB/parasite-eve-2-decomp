/* Part of the factory lift library; see factory_lift.h. */

/// Advances the lift's downward movement toward Y = 0 world units.
///
/// Requires the live lift work, model and room-owned panel-task slot. Uses
/// 16.16 position and velocity; natural completion keeps the computed position,
/// while START after `FACTORY_LIFT_SKIP_FRAMES` assigns the exact target of zero.
/// Updates local Y and invalidates composition each frame. Returns 1 when
/// already at rest or skipped, and 0 on the natural completion frame.
static s32 _factoryLiftLower(Task* task)
{
    FactoryLiftWork* work             = task->work;
    GfxCoord*        coord            = task->extra.tmd->coords;
    s32              movementComplete = 0;

    switch (work->yStep) {
        case FACTORY_LIFT_STEP_RESET:
            work->yVelocity = 0;
            work->yStep++;
            break;
        case FACTORY_LIFT_STEP_START_SOUND:
            if (gGameSession->location.loc.stage == GAME_STAGE_DRYFIELD) {
                sndEvtRequestStageScriptStart(SOUND_FACTORY_LIFT_MOVE, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
            } else {
                sndEvtRequestStageScriptStart(SOUND_NIGHT_FACTORY_LIFT_MOVE, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
            }
            work->yStep++;
            break;
        case FACTORY_LIFT_STEP_OUTBOUND:
            work->yVelocity += 0xC000;
            if (work->yVelocity > 0x30000) {
                work->yVelocity = 0x30000;
            }
            work->y.word += work->yVelocity;
            if (work->y.word > 0) {
                work->yStep++;
            }
            break;
        case FACTORY_LIFT_STEP_SETTLE:
            work->yVelocity += -0xC000;
            if (work->yVelocity < -0xC000) {
                work->yVelocity = -0xC000;
            }
            work->y.word += work->yVelocity;
            if (work->y.word <= 0) {
                // Keep the computed position; only the skip path assigns the exact target.
                _factoryLiftFinishVertical(task, coord);
                work->yStep++;
            }
            break;
        default:
            movementComplete = 1;
            break;
    }

    // Skipping is allowed only during a moving phase and after the input delay.
    if ((u8)(work->yStep - FACTORY_LIFT_STEP_START_SOUND) < FACTORY_LIFT_STEP_REST - FACTORY_LIFT_STEP_START_SOUND && padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, PAD_BUTTON_START) != 0 && work->moveFrames >= FACTORY_LIFT_SKIP_FRAMES) {
        _factoryLiftFinishVertical(task, coord);
        movementComplete = 1;
        work->y.word     = 0;
        work->yStep      = FACTORY_LIFT_STEP_REST;
    }
    coord->coord.t[1]   = work->y.halves.integer;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    return movementComplete;
}
