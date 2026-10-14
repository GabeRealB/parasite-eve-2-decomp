/* Part of the Ivory/Zebra Stalker library; see stalker_zebra_ivory.h. */

enum {
    STALKER_ZEBRA_IVORY_STATE_LIGHT_RECOIL = 3,
    STALKER_ZEBRA_IVORY_STATE_HEAVY_RECOIL = 4,
    STALKER_ZEBRA_IVORY_STATE_STATUS_HOLD  = 5,
    STALKER_ZEBRA_IVORY_STATE_CEILING_FALL = 0xE,
    STALKER_ZEBRA_IVORY_STATE_KNOCKDOWN    = 0xF
};

/// Applies a pending hit reaction to the running behavior, returning 1 for a transition.
///
/// Chooses light recoil, heavy recoil, status hold or knockdown on the floor;
/// ceiling status/knockdown selects a fall. Clears handled and unknown requests
/// and returns 0 for an unknown/empty request. Ceiling status is the exception:
/// it remains pending while the fall begins. Each transition resets `subState`.
static s32 _stalkerZebraIvoryApplyPendingReaction(Task* task)
{
    StalkerZebraIvoryWork* work;

    work = (StalkerZebraIvoryWork*)task->work;
    if (work->onCeiling == 0) {
        switch (work->pendingAction) {
            case STALKER_ZEBRA_IVORY_PENDING_LIGHT:
                _stalkerZebraIvorySelectState(task, STALKER_ZEBRA_IVORY_STATE_LIGHT_RECOIL);
                work->pendingAction = STALKER_ZEBRA_IVORY_PENDING_NONE;
                return 1;
            case STALKER_ZEBRA_IVORY_PENDING_HEAVY:
                _stalkerZebraIvorySelectState(task, STALKER_ZEBRA_IVORY_STATE_HEAVY_RECOIL);
                work->pendingAction = STALKER_ZEBRA_IVORY_PENDING_NONE;
                return 1;
            case STALKER_ZEBRA_IVORY_PENDING_STATUS:
                _stalkerZebraIvorySelectState(task, STALKER_ZEBRA_IVORY_STATE_STATUS_HOLD);
                work->pendingAction = STALKER_ZEBRA_IVORY_PENDING_NONE;
                return 1;
            case STALKER_ZEBRA_IVORY_PENDING_BLAST:
                _stalkerZebraIvorySelectState(task, STALKER_ZEBRA_IVORY_STATE_HEAVY_RECOIL);
                work->pendingAction = STALKER_ZEBRA_IVORY_PENDING_NONE;
                return 1;
            case STALKER_ZEBRA_IVORY_PENDING_KNOCKDOWN:
                _stalkerZebraIvorySelectState(task, STALKER_ZEBRA_IVORY_STATE_KNOCKDOWN);
                work->pendingAction = STALKER_ZEBRA_IVORY_PENDING_NONE;
                return 1;
        }
        work->pendingAction = STALKER_ZEBRA_IVORY_PENDING_NONE;
        return 0;
    } else {
        switch (work->pendingAction) {
            case STALKER_ZEBRA_IVORY_PENDING_LIGHT:
                _stalkerZebraIvorySelectState(task, STALKER_ZEBRA_IVORY_STATE_LIGHT_RECOIL);
                work->pendingAction = STALKER_ZEBRA_IVORY_PENDING_NONE;
                return 1;
            case STALKER_ZEBRA_IVORY_PENDING_HEAVY:
                _stalkerZebraIvorySelectState(task, STALKER_ZEBRA_IVORY_STATE_HEAVY_RECOIL);
                work->pendingAction = STALKER_ZEBRA_IVORY_PENDING_NONE;
                return 1;
            case STALKER_ZEBRA_IVORY_PENDING_STATUS:
                // Keep status pending for the falling behavior to finish handling it.
                _stalkerZebraIvorySelectState(task, STALKER_ZEBRA_IVORY_STATE_CEILING_FALL);
                return 1;
            case STALKER_ZEBRA_IVORY_PENDING_BLAST:
                _stalkerZebraIvorySelectState(task, STALKER_ZEBRA_IVORY_STATE_HEAVY_RECOIL);
                work->pendingAction = STALKER_ZEBRA_IVORY_PENDING_NONE;
                return 1;
            case STALKER_ZEBRA_IVORY_PENDING_KNOCKDOWN:
                _stalkerZebraIvorySelectState(task, STALKER_ZEBRA_IVORY_STATE_CEILING_FALL);
                work->pendingAction = STALKER_ZEBRA_IVORY_PENDING_NONE;
                return 1;
        }
        work->pendingAction = STALKER_ZEBRA_IVORY_PENDING_NONE;
        return 0;
    }
}
