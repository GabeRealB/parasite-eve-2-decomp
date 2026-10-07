/* Part of the Glutton library; see glutton.h. */

/// Refreshes the composed root coordinate of a Glutton escort model.
///
/// The escort task must own a live TMD body and coordinate. The enemy argument
/// is unused; it is retained for the enemy-state callback signature.
static void _gluttonPropTick(Enemy* enemy, Task* task)
{
    VECTOR worldPosition;

    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(task->extra.tmd->coords);
    // The original also samples this position, although no later code reads it.
    worldPosition.vx = task->extra.tmd->coords->workm.t[0];
    worldPosition.vy = task->extra.tmd->coords->workm.t[1];
    worldPosition.vz = task->extra.tmd->coords->workm.t[2];
}
