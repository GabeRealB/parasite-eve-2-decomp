/* Part of the water hole library; see water_hole.h. */

/// Initializes then draws the water hole's surfaces and publishes their height.
///
/// Task state must be 0 (prepare the borrowed actor arena) or 1 (draw).
/// Initialization advances to state 1; every callback publishes the undisplaced
/// water height in world units after dispatch. The arena and water resources
/// must remain live through drawing and GPU consumption.
static void _waterHoleWaterTask(Task* task)
{
    TaskFunc states[] = { _waterHoleWaterStart, _waterHoleDrawSurfaces };

    states[task->state](task);
    gGameSession->waterY = WATER_HOLE_SURFACE_HEIGHT;
}
