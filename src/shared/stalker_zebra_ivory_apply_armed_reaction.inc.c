/* Part of the Zebra/Ivory Stalker library; see stalker_zebra_ivory.h. */

/// Applies an armed floor status/knockdown reaction, returning 1 on transition.
///
/// Requires `pendingArmed` exactly 1 and `onCeiling` zero to take either
/// reaction. Resets `subState` on transition. Always consumes `pendingAction`,
/// including other kinds, unarmed requests and ceiling requests; preserves the
/// arm latch. The caller skips its ordinary sub-state when this returns 1.
static s32 _stalkerZebraIvoryApplyArmedReaction(Task* task)
{
    StalkerZebraIvoryWork* work = task->work;

    if (work->pendingArmed == 1 && work->onCeiling == 0) {
        switch (work->pendingAction) {
            case STALKER_ZEBRA_IVORY_PENDING_STATUS:
                _stalkerZebraIvorySelectState(task, STALKER_ZEBRA_IVORY_STATE_STATUS_HOLD);
                work->pendingAction = STALKER_ZEBRA_IVORY_PENDING_NONE;
                return 1;
            case STALKER_ZEBRA_IVORY_PENDING_KNOCKDOWN:
                _stalkerZebraIvorySelectState(task, STALKER_ZEBRA_IVORY_STATE_KNOCKDOWN);
                work->pendingAction = STALKER_ZEBRA_IVORY_PENDING_NONE;
                return 1;
        }
    }
    work->pendingAction = STALKER_ZEBRA_IVORY_PENDING_NONE;
    return 0;
}
