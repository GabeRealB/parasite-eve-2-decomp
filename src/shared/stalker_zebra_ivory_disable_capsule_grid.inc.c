/* Part of the Ivory/Zebra Stalker library; see stalker_zebra_ivory.h. */

/// Disables room-grid testing for the probe capsule while keeping its body linked.
static void _stalkerZebraIvoryDisableCapsuleGrid(Task* task)
{
    StalkerZebraIvoryWork* work = (StalkerZebraIvoryWork*)task->work;

    work->capsuleBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
}
