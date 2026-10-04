/* Part of the Glutton library; see glutton.h. */

/// Walk the pitch `neckPitch` toward `arg1` (clamped to 0..0x500) by at most
/// 0x10 per call, then pitch model parts 3 and 4 about x: part 3 to half of
/// it, part 4 against it, each net of the pitch it already has.
void gluttonPitchNeck(Task* task, s16 arg1)
{
    GluttonWork* work = task->work;
    s16          value;
    s16          pitch4;
    s16          pitch3;

    value = arg1;
    if (arg1 > 0x500) {
        value = 0x500;
    }
    if (arg1 < 0) {
        value = 0;
    }

    if (work->neckPitch < value) {
        if (value - work->neckPitch >= 0x11) {
            work->neckPitch = work->neckPitch + 0x10;
        } else {
            work->neckPitch = value;
        }
    } else if (value < work->neckPitch) {
        if (abs(work->neckPitch - value) >= 0x11) {
            work->neckPitch = work->neckPitch - 0x10;
        } else {
            work->neckPitch = value;
        }
    }

    pitch4 = -ratan2(task->extra.tmd->coords[4].coord.m[1][2],
                     task->extra.tmd->coords[4].coord.m[2][2]);
    pitch3 = -ratan2(task->extra.tmd->coords[3].coord.m[1][2],
                     task->extra.tmd->coords[3].coord.m[2][2]);

    gfxRotMatrixX(&task->extra.tmd->coords[3].coord, work->neckPitch / 2 - pitch3, GRAPHICS_ROTATION_COMPOSE);
    task->extra.tmd->coords[3].composeStamp = GRAPHICS_COORD_DIRTY;
    gfxRotMatrixX(&task->extra.tmd->coords[4].coord, -work->neckPitch - pitch4, GRAPHICS_ROTATION_COMPOSE);
    task->extra.tmd->coords[4].composeStamp = GRAPHICS_COORD_DIRTY;
}
