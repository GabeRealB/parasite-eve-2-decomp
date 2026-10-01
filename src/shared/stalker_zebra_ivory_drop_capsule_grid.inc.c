/* Part of the Ivory/Zebra Stalker library; see stalker_zebra_ivory.h. */

/// Takes the capsule body out of the collision grid.
void stalkerZebraIvoryDropCapsuleGrid(Task* arg0)
{
    StalkerZebraIvoryWork* work = (StalkerZebraIvoryWork*)arg0->work;

    work->capsuleBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
}
