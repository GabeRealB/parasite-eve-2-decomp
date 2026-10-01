/* Part of the Diver library; see diver.h. */

/// Moves the actor's root coordinate `dist` units along heading `yaw` in the XZ
/// plane and marks it dirty.
void diverStepForward(Task* task, s16 arg1, s16 arg2)
{
    task->extra.tmd->coords->coord.t[0]  += ((rsin(arg2) << 4) * arg1) >> 16;
    task->extra.tmd->coords->coord.t[2]  += ((rcos(arg2) << 4) * arg1) >> 16;
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
}
