/* Part of the Mad Chaser library; see mad_chaser.h. */

// Bind MAD_CHASER_ALERT_STATE_HANDLER to this carrier's declared static
// void(Task*) callback before inclusion and undefine it afterwards. The binding
// evaluates no arguments, captures no values and constructs no tokens.
// Each carrier must first define `_gMadChaserAlertSteps` as a readable,
// complete `TaskFuncTable5` of the five alert phases in dispatch order.

/// Dispatches one phase of the combat alert cry and sidestep.
///
/// Borrows live `MadChaserWork`; `subState` must be 0 cry, 1 wait, 2 release
/// the shared alert claim, 3 start the sidestep or 4 sidestep. There is no bounds
/// check. Copies all five non-NULL void(Task*) callbacks before reading the
/// index as a signed halfword and calling one handler. Their code must remain
/// loaded through the call. The phase handler owns transitions; the combat
/// caller advances animation and applies collision afterwards. This dispatcher
/// retains no task, work or table storage.
static void MAD_CHASER_ALERT_STATE_HANDLER(Task* task)
{
    MadChaserWork* work;
    TaskFuncTable5 stepHandlers;

    work         = task->work;
    stepHandlers = _gMadChaserAlertSteps;
    stepHandlers.funcs[(s16)work->subState](task);
}
