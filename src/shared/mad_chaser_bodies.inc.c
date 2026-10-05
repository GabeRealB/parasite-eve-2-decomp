/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Links the actor's three collision
/// objects onto `Gp_ObjLists[2]` and clears their record tables.
void madChaserLinkBodies(Task* arg0)
{
    MadChaserWork* work = (MadChaserWork*)arg0->work;

    work->pairBody.coord            = &arg0->extra.tmd->coords[1];
    work->pairBody.context.contacts = work->contacts;
    work->pairBody.pos.vx           = 0;
    work->pairBody.pos.vy           = 0;
    work->pairBody.pos.vz           = 0;
    work->pairBody.key              = 0x3002C;
    work->pairBody.radius           = 0x170;
    work->pairBody.flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(2, &work->pairBody);
    worldCollisionInitContacts(work->contacts, ARRAY_SIZE(work->contacts), 0);
    work->pairBody.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;

    work->attackBody.coord            = &arg0->extra.tmd->coords[1];
    work->attackBody.context.contacts = work->attackContacts;
    work->attackBody.pos.vx           = 0;
    work->attackBody.pos.vy           = 0;
    work->attackBody.pos.vz           = 0;
    work->attackBody.key              = Gp_PackObjPair(arg0->spawnArg2.pointer, 0);
    work->attackBody.radius           = 0x170;
    work->attackBody.flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(2, &work->attackBody);
    worldCollisionInitContacts(work->attackContacts, ARRAY_SIZE(work->attackContacts), 0);
    work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);

    work->gridBody.coord            = &arg0->extra.tmd->coords[1];
    work->gridBody.context.contacts = work->contacts;
    work->gridBody.pos.vx           = 0;
    work->gridBody.pos.vy           = 0;
    work->gridBody.pos.vz           = 0;
    work->gridBody.key              = 0x3002C;
    work->gridBody.radius           = 0x224;
    work->gridBody.flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(2, &work->gridBody);
    work->gridBody.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
}
