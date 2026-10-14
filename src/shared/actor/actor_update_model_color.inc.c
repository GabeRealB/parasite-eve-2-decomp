/* Shared by actor_00300 and actor_105100. */

/// Updates the enemy's model lighting and colour at its composed root position.
///
/// Requires a live enemy in `spawnArg2.pointer` and a composed model root.
/// Only the three world-coordinate components are sampled; colour modes and
/// blends are applied by the world-lighting service.
static inline void _actorUpdateModelColor(Task* task)
{
    GfxCoord* coord;
    VECTOR3   worldPosition;

    coord            = task->extra.tmd->coords;
    worldPosition.vx = coord->workm.t[0];
    worldPosition.vy = coord->workm.t[1];
    worldPosition.vz = coord->workm.t[2];
    worldCoordUpdateActorColor(task->spawnArg2.pointer, &worldPosition, 0, 0);
}
