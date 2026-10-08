/* Part of the factory lift library; see factory_lift.h. */

/// Tears down the lift and its hatch child through the default task cleanup.
///
/// Releases the lift's owned work and model resources. The borrowed panel
/// slot and the room's collision pools remain owned by the room.
static void _factoryLiftExit(Task* task)
{
    taskKill(task);
}
