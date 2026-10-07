/* Part of the Glutton library; see glutton.h. */

/// Advances the host's requested vertical screen shake by one frame.
///
/// A changed supported level arms 5, 10 or 22 frames; the post-decrement count
/// selects the short, medium or long pixel-offset pattern. Completion clears
/// both levels and the display offset on the following tick. An unsupported
/// changed level returns without arming or clearing the current display offset.
/// The dumping-hole encounter tolerates NULL work; the incinerator requires
/// live `GluttonWork`.
static void _gluttonShakeTick(Task* task)
{
    GluttonWork* work = task->work;
    s32          phase;
#if GLUTTON_ROOM == GLUTTON_DUMPING_HOLE

    if (work == NULL) {
        return;
    }
#endif

    // Only a changed supported request restarts the frame count.
    if (work->shakeLevel != work->armedShakeLevel) {
        switch (work->shakeLevel) {
            case GLUTTON_SHAKE_SHORT:
                work->shakeFramesRemaining = 5;
                break;
            case GLUTTON_SHAKE_MEDIUM:
                work->shakeFramesRemaining = 10;
                break;
            case GLUTTON_SHAKE_LONG:
                work->shakeFramesRemaining = 22;
                break;
            case GLUTTON_SHAKE_NONE:
            default:
                return;
        }
        work->armedShakeLevel = work->shakeLevel;
    }

    if (work->shakeFramesRemaining == 0) {
        displaySetShakeY(0);
        work->shakeLevel      = GLUTTON_SHAKE_NONE;
        work->armedShakeLevel = GLUTTON_SHAKE_NONE;
        return;
    }
    work->shakeFramesRemaining--;

    switch (work->shakeLevel) {
        case GLUTTON_SHAKE_SHORT:
            phase = work->shakeFramesRemaining;
            if ((phase & 1) == 0) {
                work->shakeY = 0;
            } else {
                work->shakeY = 2;
            }
            displaySetShakeY(work->shakeY);
            break;

        case GLUTTON_SHAKE_MEDIUM:
            phase = work->shakeFramesRemaining;
            switch (phase & 3) {
                case 0:
                    work->shakeY = 0;
                    break;
                case 1:
                    work->shakeY = 2;
                    break;
                case 2:
                    work->shakeY = 3;
                    break;
                case 3:
                    work->shakeY = 2;
                    break;
            }
            displaySetShakeY(work->shakeY);
            break;

        case GLUTTON_SHAKE_LONG:
            phase = work->shakeFramesRemaining;
            switch (phase & 7) {
                case 3:
                case 4:
                    work->shakeY = 4;
                    break;
                case 2:
                case 5:
                    work->shakeY = 3;
                    break;
                case 1:
                case 6:
                    work->shakeY = 1;
                    break;
                case 0:
                case 7:
                    work->shakeY = 0;
                    break;
            }
            displaySetShakeY(work->shakeY);
            break;

        case GLUTTON_SHAKE_NONE:
        default:
            displaySetShakeY(0);
            break;
    }
}
