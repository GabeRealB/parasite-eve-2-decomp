/* Part of the Glutton library; see glutton.h. */

/// Moves the neck toward a target pitch and applies it to the two neck parts.
///
/// `pitchTarget` uses 4096 angle units per turn, clamped to 0..0x500. The stored
/// pitch closes by at most 0x10 per call. Host parts 3 and 4 must be live; their
/// local X rotations become half the pitch and its negation respectively,
/// relative to the current animation pose. Both composition caches are dirtied.
static void _gluttonPitchNeck(Task* task, s16 pitchTarget)
{
    enum { GLUTTON_NECK_PITCH_LIMIT = 0x500,
           GLUTTON_NECK_PITCH_STEP  = 0x10 };
    GluttonWork* work = task->work;
    s16          clampedPitch;
    s16          tipPitch;
    s16          basePitch;

    clampedPitch = pitchTarget;
    if (pitchTarget > GLUTTON_NECK_PITCH_LIMIT) {
        clampedPitch = GLUTTON_NECK_PITCH_LIMIT;
    }
    if (pitchTarget < 0) {
        clampedPitch = 0;
    }

    if (work->neckPitch < clampedPitch) {
        if (clampedPitch - work->neckPitch >= GLUTTON_NECK_PITCH_STEP + 1) {
            work->neckPitch = work->neckPitch + GLUTTON_NECK_PITCH_STEP;
        } else {
            work->neckPitch = clampedPitch;
        }
    } else if (clampedPitch < work->neckPitch) {
        if (abs(work->neckPitch - clampedPitch) >= GLUTTON_NECK_PITCH_STEP + 1) {
            work->neckPitch = work->neckPitch - GLUTTON_NECK_PITCH_STEP;
        } else {
            work->neckPitch = clampedPitch;
        }
    }

    // Replace the animated X pitches without disturbing the other axes.
    tipPitch  = -ratan2(task->extra.tmd->coords[4].coord.m[1][2],
                        task->extra.tmd->coords[4].coord.m[2][2]);
    basePitch = -ratan2(task->extra.tmd->coords[3].coord.m[1][2],
                        task->extra.tmd->coords[3].coord.m[2][2]);

    gfxRotMatrixX(&task->extra.tmd->coords[3].coord, work->neckPitch / 2 - basePitch, GRAPHICS_ROTATION_COMPOSE);
    task->extra.tmd->coords[3].composeStamp = GRAPHICS_COORD_DIRTY;
    gfxRotMatrixX(&task->extra.tmd->coords[4].coord, -work->neckPitch - tipPitch, GRAPHICS_ROTATION_COMPOSE);
    task->extra.tmd->coords[4].composeStamp = GRAPHICS_COORD_DIRTY;
}
