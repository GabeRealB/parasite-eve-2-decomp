/* Part of the Glutton library; see glutton.h. */

/// Walk the pitch `field_F00` toward `arg1` (clamped to 0..0x500) by at most
/// 0x10 per call, then pitch model parts 3 and 4 about x: part 3 to half of
/// it, part 4 against it, each net of the pitch it already has.
void gluttonPitchNeck(Task* task, s16 arg1)
{
    Actor403200Work* work = task->work;
    s16              value;
    s16              pitch4;
    s16              pitch3;

    value = arg1;
    if (arg1 > 0x500) {
        value = 0x500;
    }
    if (arg1 < 0) {
        value = 0;
    }

    if (work->field_F00 < value) {
        if (value - work->field_F00 >= 0x11) {
            work->field_F00 = work->field_F00 + 0x10;
        } else {
            work->field_F00 = value;
        }
    } else if (value < work->field_F00) {
        if (abs(work->field_F00 - value) >= 0x11) {
            work->field_F00 = work->field_F00 - 0x10;
        } else {
            work->field_F00 = value;
        }
    }

    pitch4 = -ratan2(task->extra.tmd->coords[4].coord.m[1][2],
                     task->extra.tmd->coords[4].coord.m[2][2]);
    pitch3 = -ratan2(task->extra.tmd->coords[3].coord.m[1][2],
                     task->extra.tmd->coords[3].coord.m[2][2]);

    Gfx_RotMatrixX(&task->extra.tmd->coords[3].coord, work->field_F00 / 2 - pitch3, 0);
    task->extra.tmd->coords[3].composeStamp = GRAPHICS_COORD_DIRTY;
    Gfx_RotMatrixX(&task->extra.tmd->coords[4].coord, -work->field_F00 - pitch4, 0);
    task->extra.tmd->coords[4].composeStamp = GRAPHICS_COORD_DIRTY;
}
