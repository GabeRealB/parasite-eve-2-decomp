/* Part of the water hole library; see water_hole.h. */

/// The room's water task: runs its current state -
/// `_waterHoleWaterStart` once, then
/// `_waterHoleDrawSurfaces`, which draws the surfaces - and each
/// tick sets the session's water height to -0x1A4.
void waterHoleWaterTask(Task* task)
{
    TaskFunc states[2] = { _waterHoleWaterStart, _waterHoleDrawSurfaces };

    states[task->state](task);
    gGameSession->waterY = -0x1A4;
}
