#include "main/random.h"

/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Waits through the lurk idle hold, interrupting it when the nearer player comes close.
///
/// Expires when the old elapsed signed halfword exceeds holdFrames, before its
/// post-increment. Expiration selects
/// look or shift with equal random probability. Before expiration, distance below
/// 3500 parent-coordinate units starts alert; below 5000 advances to the idle-end
/// sub-state. Requires initialized Mad Chaser work and refreshed player distance.
static void _madChaserLurkWait(Task* task)
{
    MadChaserWork* work = task->work;
    s16            playerDistance;

    if (work->holdFrames < (s16)work->stateFrames++) {
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        if ((gRandomLcgState >> 16) & 1) {
            MadChaserWork* nextWork = task->work;

            nextWork->state    = MAD_CHASER_LURK_STATE_SHIFT;
            nextWork->subState = 0;
        } else {
            MadChaserWork* nextWork = task->work;

            nextWork->state    = MAD_CHASER_LURK_STATE_LOOK;
            nextWork->subState = 0;
        }
        return;
    }
    playerDistance = work->playerDist;
    if (playerDistance < MAD_CHASER_LURK_ALERT_DISTANCE) {
        MadChaserWork* alertWork = task->work;

        alertWork->state    = MAD_CHASER_LURK_STATE_ALERT;
        alertWork->subState = 0;
        return;
    }
    if (playerDistance < MAD_CHASER_LURK_LOOK_DISTANCE) {
        work->subState++;
    }
}
