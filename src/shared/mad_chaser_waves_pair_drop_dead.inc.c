/* Part of the Mad Chaser waves library; see mad_chaser_waves.h. */

/// Forgets dead encounter-pair enemies and marks their slots gone on the next check.
///
/// Requires live pair work and borrowed enemies. HP at or below zero clears
/// each non-NULL pointer; a pointer already NULL sets its goneMask bit.
/// Clearing and marking occur on successive calls. Enemy tasks remain owned
/// by the actor system; this check neither destroys them nor reads workType.
static void _overlayEncounterForgetDeadPairMembers(Task* task)
{
    OverlayEncounterPairWork* work = task->work;

    if (work->enemy0 != NULL) {
        if (work->enemy0->hp <= 0) {
            work->enemy0 = NULL;
        }
    } else {
        work->goneMask |= OVERLAY_ENCOUNTER_PAIR_GONE_ENEMY0;
    }
    if (work->enemy1 != NULL) {
        if (work->enemy1->hp <= 0) {
            work->enemy1 = NULL;
        }
    } else {
        work->goneMask |= OVERLAY_ENCOUNTER_PAIR_GONE_ENEMY1;
    }
}
