/* Part of the Mad Chaser library; see mad_chaser.h. */

// MAD_CHASER_COMBAT_ENTER_ALERT_HANDLER selects a carrier's declared static
// void(Task*) callback for combat table slot 1 or 2. Bind it around each extra
// inclusion and undefine it afterwards. This object-like identifier binding
// evaluates no arguments, captures no values and constructs no tokens.
// Without a binding, this fragment defines the existing slot-0 callback.
#ifdef MAD_CHASER_COMBAT_ENTER_ALERT_HANDLER
/// Redirects combat behavior 1 or 2 to the alert's first step.
///
/// Borrows the task's live `MadChaserWork` and selects
/// `MAD_CHASER_COMBAT_STATE_ALERT` with sub-state zero. The combat dispatcher
/// runs that step on its next dispatch; task state, animation requests, counters
/// and ownership remain intact.
static void MAD_CHASER_COMBAT_ENTER_ALERT_HANDLER(Task* task)
#else
/// Sets the state machine to state 5, the alert, with `subState` cleared.
void madChaserToAlertState(Task* task)
#endif
{
    _madChaserSetBehaviorState(task, MAD_CHASER_COMBAT_STATE_ALERT);
}
