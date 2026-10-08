/* Part of the Ivory/Zebra Stalker library; see stalker_zebra_ivory.h. */

/// Runs the Stalker's knockdown recoil, rest transition or return to movement.
///
/// Requires initialized shared work and subState 0..2 for the carrier's
/// three-entry knockdown table. Clears both arm-extension requests first.
/// An armed floor status/knockdown reaction consumes the pending action and
/// takes precedence; otherwise the selected sub-state runs once. The task,
/// model and carrier's animation resources remain owned by the Stalker.
static void _stalkerZebraIvoryRunSubStates(Task* task)
{
    StalkerZebraIvoryWork* work      = task->work;
    TaskFuncTable3         subStates = gStalkerZebraIvorySubStates;

    _stalkerZebraIvoryFoldArms(task);
    if ((_stalkerZebraIvoryApplyArmedReaction(task) << 0x10) == 0) {
        subStates.funcs[work->subState](task);
    }
}
