/* Part of the Mad Chaser library; see mad_chaser.h. */

// Bind MAD_CHASER_STEP_STATE to a declared void(Task*) callback before each
// inclusion, then undefine it. The prior declaration establishes its linkage.
// This object-like binding names only the definition; it evaluates no arguments
// and performs no token construction. The carrier also supplies the three-entry
// gMadChaserWalkSteps table and an s16(Task*) MAD_CHASER_WALK_INTERRUPT_HANDLER.

/// Runs the current behavior step unless its interrupt handler suppresses it.
///
/// Requires live Mad Chaser work and `subState` in 0..2 with all three callbacks
/// loaded. The interrupt runs once: combat walk consumes a hit reaction; lurk
/// idle and look join a claimed shared alert. A nonzero signed-halfword result
/// suppresses this frame's step. A step may change the behavior or sub-state;
/// the selected table is copied before the interrupt and is not reselected.
void MAD_CHASER_STEP_STATE(Task* task)
{
    MadChaserWork* work         = task->work;
    TaskFuncTable3 stepHandlers = gMadChaserWalkSteps;

    if (MAD_CHASER_WALK_INTERRUPT_HANDLER(task) == 0) {
        stepHandlers.funcs[(s16)work->subState](task);
    }
}
