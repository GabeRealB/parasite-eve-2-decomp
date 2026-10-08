/* Part of the factory lift library; see factory_lift.h. */

/// Records the lift's completed movement for the operator panel to consume.
///
/// Handles `FACTORY_PANEL_MESSAGE_MOVE_SETTLED` for a live panel work block.
/// The sender supplies zero payloads and discards the result; this callback
/// defines no return word. All argument words except the receiver are ignored.
static void _factoryPanelMarkMoveSettled(Task* task, s32 messageId, s32 firstArg, s32 secondArg)
{
    FactoryPanelWork* work = task->work;

    work->moveSettled = 1;
}
