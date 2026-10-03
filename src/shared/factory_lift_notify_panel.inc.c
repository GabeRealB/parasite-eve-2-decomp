/* Part of the factory lift library; see factory_lift.h. */

/// Sends message 0x13F3 to `arg0`, if there is one.
void factoryLiftNotifyPanel(Task* arg0)
{
    if (arg0 != NULL) {
        taskMessageDispatch(arg0, FACTORY_PANEL_MESSAGE_MOVE_SETTLED, 0, 0);
    }
}
