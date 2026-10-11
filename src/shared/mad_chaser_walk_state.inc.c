/* Part of the Mad Chaser library; see mad_chaser.h. */

// Bind MAD_CHASER_STEP_STATE to a declared void(Task*) callback before each
// inclusion, then undefine it. The prior declaration establishes its linkage.
// This object-like binding names only the definition; it evaluates no arguments
// and performs no token construction. Bind MAD_CHASER_STEP_TABLE to a readable
// TaskFuncTable3 value for the combat walk, lurk idle or lurk look instance.
// Its three non-NULL void(Task*) callbacks must remain loaded through dispatch.
// This object-like binding is evaluated once as the table copy; it constructs
// no tokens. Undefine it after inclusion.
// Bind MAD_CHASER_STEP_INTERRUPT_HANDLER to a declared s16(Task*) predicate
// taking a task with live MadChaserWork, and keep its code loaded. This
// object-like binding selects one direct call with task, after the table copy
// and before reading subState: zero continues dispatch, nonzero skips the step.
// The combat walk binds the hit-request consumer; lurk idle/look bind the
// shared-alert joiner. It constructs no tokens and has no default; undefine it
// after inclusion.

#ifndef MAD_CHASER_STEP_TABLE
#error "Bind MAD_CHASER_STEP_TABLE to this instance's TaskFuncTable3 value before inclusion"
#endif

#ifndef MAD_CHASER_STEP_INTERRUPT_HANDLER
#error "Bind MAD_CHASER_STEP_INTERRUPT_HANDLER to this instance's s16(Task*) predicate before inclusion"
#endif

/// Dispatches one Mad Chaser behavior step unless an interrupt takes over.
///
/// Borrows the task's live `MadChaserWork`; `subState` must be in 0..2 and all
/// three callbacks must be loaded. Copies the complete step table before
/// calling the interrupt once, then reads the signed-halfword step index only
/// if the interrupt returns zero. A nonzero s16 result skips this frame's step.
///
/// The combat walk instance starts the approach at step 0, approaches the
/// tracked player at step 1, and finishes the walk cycle before selecting leap
/// at step 2. A hit latch equal to one consumes the pending reaction and skips
/// the step even when no reaction is selected. The caller tracks the player
/// before dispatch and advances animation and applies collision afterwards.
/// The lurk idle/look instances instead interrupt to join a claimed alert.
static void MAD_CHASER_STEP_STATE(Task* task)
{
    MadChaserWork* work         = task->work;
    TaskFuncTable3 stepHandlers = MAD_CHASER_STEP_TABLE;

    if (MAD_CHASER_STEP_INTERRUPT_HANDLER(task) == 0) {
        stepHandlers.funcs[(s16)work->subState](task);
    }
}
