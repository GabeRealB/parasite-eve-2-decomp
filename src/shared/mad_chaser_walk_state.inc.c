/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Runs a three-entry sub-state table unless its interrupt handler takes over.
///
/// The carrier must bind `MAD_CHASER_WALK_INTERRUPT_HANDLER` to a declared
/// s16 predicate taking Task*. Combat binds the hit-reaction consumer;
/// lurk hold and crouch bind the alert joiner. The handler is called once,
/// and a nonzero low halfword suppresses sub-state dispatch for this frame.
void madChaserWalkState(Task* arg0)
{
    MadChaserWork* work;
    TaskFuncTable3 sp;

    work = (MadChaserWork*)arg0->work;
    sp   = gMadChaserWalkSteps;
    if (MAD_CHASER_WALK_INTERRUPT_HANDLER(arg0) == 0) {
        sp.funcs[(s16)work->subState](arg0);
    }
}
