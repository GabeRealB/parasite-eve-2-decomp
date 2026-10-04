/* Part of the factory lift library; see factory_lift.h. */

/// The handler the model runs while bit 1 of game flag 0x49 is clear, and --
/// when bit 0 is set with it -- the handler that follows.
s32 factoryLiftLower(Task* task)
{
    FactoryLiftWork* work  = task->work;
    GfxCoord*        coord = task->extra.tmd->coords;
    s32              done  = 0;

    switch (work->yStep) {
        case 0:
            work->yVelocity = 0;
            work->yStep++;
            break;
        case 1:
            if (gGameSession->location.loc.stage == GAME_STAGE_DRYFIELD) {
                Gp_EnqueueStageSnd6(SOUND_FACTORY_LIFT_MOVE, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
            } else {
                Gp_EnqueueStageSnd6(SOUND_NIGHT_FACTORY_LIFT_MOVE, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
            }
            work->yStep++;
            break;
        case 2:
            work->yVelocity += 0xC000;
            if (work->yVelocity > 0x30000) {
                work->yVelocity = 0x30000;
            }
            work->y.word += work->yVelocity;
            if (work->y.word > 0) {
                work->yStep++;
            }
            break;
        case 3:
            work->yVelocity += -0xC000;
            if (work->yVelocity < -0xC000) {
                work->yVelocity = -0xC000;
            }
            work->y.word += work->yVelocity;
            if (work->y.word <= 0) {
                factoryLiftNotifyPanel(*(Task**)task->spawnArg2.pointer);
                if (gGameSession->location.loc.stage == GAME_STAGE_DRYFIELD) {
                    Gp_EnqueueStageSnd7(SOUND_FACTORY_LIFT_MOVE, 1);
                    Gp_EnqueueStageSnd6(SOUND_FACTORY_LIFT_MOVE_STOP, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
                } else {
                    Gp_EnqueueStageSnd7(SOUND_NIGHT_FACTORY_LIFT_MOVE, 1);
                    Gp_EnqueueStageSnd6(SOUND_NIGHT_FACTORY_LIFT_MOVE_STOP, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
                }
                work->yStep++;
            }
            break;
        default:
            done = 1;
            break;
    }

    if ((u8)(work->yStep - 1) < 3 && padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, PAD_BUTTON_START) != 0 && work->moveFrames >= FACTORY_LIFT_SKIP_FRAMES) {
        factoryLiftNotifyPanel(*(Task**)task->spawnArg2.pointer);
        if (gGameSession->location.loc.stage == GAME_STAGE_DRYFIELD) {
            Gp_EnqueueStageSnd7(SOUND_FACTORY_LIFT_MOVE, 1);
            Gp_EnqueueStageSnd6(SOUND_FACTORY_LIFT_MOVE_STOP, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
        } else {
            Gp_EnqueueStageSnd7(SOUND_NIGHT_FACTORY_LIFT_MOVE, 1);
            Gp_EnqueueStageSnd6(SOUND_NIGHT_FACTORY_LIFT_MOVE_STOP, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
        }
        done         = 1;
        work->y.word = 0;
        work->yStep  = 4;
    }
    coord->coord.t[1]   = work->y.halves.integer;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    return done;
}
