/* Part of the factory lift library; see factory_lift.h. */

/// Keeps the factory room's entry task alive to receive room messages.
///
/// The initialized task retains its message table and panel-task slot until
/// teardown; this per-frame callback leaves them and the state unchanged.
static void _factoryEntryIdle(Task* task)
{
}
