/* Part of the Glutton library; see glutton.h. */

/// Screen-shake driver for the enemy task: `gluttonSetShakeLevel` writes a
/// level into `shakeLevel`, and a change from the armed level in `armedShakeLevel`
/// starts a shake of 5, 10 or 22 frames -- any other level is ignored. Each tick
/// spends one frame and drives `displaySetShakeY` off the frame counter's
/// low bits, so `GLUTTON_SHAKE_SHORT` alternates 0 / 2, `GLUTTON_SHAKE_MEDIUM`
/// walks a four-frame 0 / 2 / 3 / 2 pattern and `GLUTTON_SHAKE_LONG` an
/// eight-frame ramp that peaks at 4. The shake clears
/// itself once the counter runs out. The dumping-hole build also checks for
/// a NULL work block before updating the shake.
void gluttonShakeTick(Task* arg0)
{
    GluttonWork* work = arg0->work;
    s32          phase;
#if GLUTTON_ROOM == GLUTTON_DUMPING_HOLE

    work = arg0->work;
    if (work == NULL) {
        return;
    }
#endif

    if (work->shakeLevel != work->armedShakeLevel) {
        switch (work->shakeLevel) {
            case GLUTTON_SHAKE_SHORT:
                work->shakeFramesRemaining = 5;
                break;
            case GLUTTON_SHAKE_MEDIUM:
                work->shakeFramesRemaining = 0xA;
                break;
            case GLUTTON_SHAKE_LONG:
                work->shakeFramesRemaining = 0x16;
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
