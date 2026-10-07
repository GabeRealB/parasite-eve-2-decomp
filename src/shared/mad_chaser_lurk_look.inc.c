/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Turns and sways the spine during the lurk look hold, engaging battle after 16 qualifying ticks.
///
/// Increments the elapsed halfword before testing it; exceeding the hold selects
/// the rise behavior. The final 48 hold ticks ease the yaw to zero. Earlier ticks
/// track a player within 3500 parent-coordinate units and 960 heading units of
/// forward, or sway about +/-896 yaw units. Angles use 4096 units per turn.
/// Qualifying ticks accumulate across interruptions; they need not be consecutive.
/// Requires initialized Mad Chaser work and refreshed player distance/bearing.
static void _madChaserLurkLookAround(Task* task)
{
    enum {
        MAD_CHASER_LOOK_RELAX_FRAMES   = 48,
        MAD_CHASER_LOOK_ENGAGE_TICKS   = 16,
        MAD_CHASER_LOOK_FORWARD_LIMIT  = 960,
        MAD_CHASER_LOOK_BACKWARD_LIMIT = 3136,
        MAD_CHASER_LOOK_SWAY_YAW       = 896,
    };
    MadChaserWork* work = task->work;
    MadChaserWork* nextWork;
    MadChaserWork* alertWork;
    s32            spineYaw;
    s32            relaxYaw;
    s32            playerBearing;

    if ((s16)++work->stateFrames > work->holdFrames) {
        nextWork           = task->work;
        nextWork->state    = MAD_CHASER_LURK_STATE_RISE;
        nextWork->subState = 0;
        return;
    }
    // Relax the spine before the hold ends and the rise transition begins.
    if (work->holdFrames - MAD_CHASER_LOOK_RELAX_FRAMES < (s16)work->stateFrames) {
        relaxYaw       = (u16)work->spineYaw;
        work->spineYaw = relaxYaw + ((s16)(-(relaxYaw * 16)) >> 9);
        return;
    }
    if (work->playerDist < MAD_CHASER_LURK_ALERT_DISTANCE && (playerBearing = (u16)work->playerBearing, (playerBearing < MAD_CHASER_LOOK_FORWARD_LIMIT || playerBearing > MAD_CHASER_LOOK_BACKWARD_LIMIT))) {
        spineYaw       = (u16)work->spineYaw;
        work->spineYaw = spineYaw + ((s16)((playerBearing - spineYaw) * 16) >> 6);
        if (++work->lookFrames >= MAD_CHASER_LOOK_ENGAGE_TICKS) {
            sceneEngageBattle(1);
            alertWork           = task->work;
            alertWork->state    = MAD_CHASER_LURK_STATE_ALERT;
            alertWork->subState = 0;
        }
    } else {
        // Alternate sway targets every 64 running ticks; keep the halfword wrap.
        if (!(((u16)work->frameCount >> 6) & 1)) {
            work->spineYaw = (u16)work->spineYaw + ((s16)((MAD_CHASER_LOOK_SWAY_YAW * 16) - (u16)work->spineYaw * 16) >> 9);
        } else {
            work->spineYaw = (u16)work->spineYaw + ((s16)(-(MAD_CHASER_LOOK_SWAY_YAW * 16) - (u16)work->spineYaw * 16) >> 9);
        }
    }
}
