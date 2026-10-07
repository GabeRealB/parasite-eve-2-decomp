/* Part of the factory lift library; see factory_lift.h. */

/// Reports that the lift movement has settled to the active operator panel.
///
/// A null `panelTask` means no panel session is open. A live receiver must
/// accept `FACTORY_PANEL_MESSAGE_MOVE_SETTLED`; both payload words are zero.
/// Dispatch is synchronous and its undefined result is discarded.
static void _factoryLiftNotifyPanel(Task* panelTask)
{
    if (panelTask != NULL) {
        taskMessageDispatch(panelTask, FACTORY_PANEL_MESSAGE_MOVE_SETTLED, 0, 0);
    }
}
