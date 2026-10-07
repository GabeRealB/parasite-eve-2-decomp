/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Consumes hit reactions that can replace an active knockdown behavior.
///
/// Requires live Mad Chaser work in combat. Only hitTaken == 1 consumes the
/// request: status selects behavior 8, knockdown selects 9, and both reset
/// subState. Other reactions only clear hitReaction. Returns an s32 boolean,
/// 1 for the exact-one latch even for an absent/unsupported reaction, otherwise
/// 0. The hit latch, counters and task state are retained; playback is unchanged.
static s32 _madChaserTakeKnockdownRequest(Task* task)
{
    MadChaserWork* work = task->work;

    if (work->hitTaken == 1) {
        switch (work->hitReaction) {
            case MAD_CHASER_HIT_REACTION_STATUS:
                work->state    = MAD_CHASER_COMBAT_STATE_STATUS_HOLD;
                work->subState = 0;
                break;
            case MAD_CHASER_HIT_REACTION_KNOCKDOWN:
                work->state    = MAD_CHASER_COMBAT_STATE_KNOCKDOWN;
                work->subState = 0;
                break;
        }
        work->hitReaction = MAD_CHASER_HIT_REACTION_NONE;
        return 1;
    }
    return 0;
}
