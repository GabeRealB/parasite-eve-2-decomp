/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Attaches a sphere at model part 1's origin with borrowed contact storage.
///
/// taskArg is a live Task*, sphereArg a writable WorldCollisionBody* and
/// contactsArg a live WorldCollisionContact* table retained until unlinking.
/// taskArg and contactsArg are evaluated once; sphereArg five times. Supply
/// stable expressions without side effects. Expands to one compound statement.
#define MAD_CHASER_SET_SPHERE_ORIGIN(taskArg, sphereArg, contactsArg)     \
    {                                                                     \
        (sphereArg)->coord            = &(taskArg)->extra.tmd->coords[1]; \
        (sphereArg)->context.contacts = (contactsArg);                    \
        (sphereArg)->pos.vx           = 0;                                \
        (sphereArg)->pos.vy           = 0;                                \
        (sphereArg)->pos.vz           = 0;                                \
    }

/// Initializes and links the Mad Chaser's hit, attack and room-grid spheres.
///
/// All three borrow model part 1's coordinate and work-owned contact storage.
/// The 368-unit hit and attack spheres start with pair tests on and off,
/// respectively; the 548-unit grid sphere has only grid tests enabled and shares
/// the hit sphere's eight contacts. The attack uses enemy attack definition 0
/// and its own two contacts. Requires zeroed live work, a nine-part model and
/// an Enemy spawn argument; unlink every sphere before freeing either owner.
static void _madChaserLinkBodies(Task* task)
{
    enum {
        MAD_CHASER_BODY_ID     = 0x2C,
        MAD_CHASER_PAIR_RADIUS = 368,
        MAD_CHASER_GRID_RADIUS = 548,
    };
    MadChaserWork* work = task->work;

    MAD_CHASER_SET_SPHERE_ORIGIN(task, &work->pairBody, work->contacts);
    work->pairBody.key    = WORLD_COLLISION_CONTACT_ENEMY_BODY | MAD_CHASER_BODY_ID;
    work->pairBody.radius = MAD_CHASER_PAIR_RADIUS;
    work->pairBody.flags  = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, &work->pairBody);
    worldCollisionInitContacts(work->contacts, ARRAY_SIZE(work->contacts), 0);
    work->pairBody.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;

    MAD_CHASER_SET_SPHERE_ORIGIN(task, &work->attackBody, work->attackContacts);
    work->attackBody.key    = damagePackEnemyAttackKey(task->spawnArg2.pointer, 0);
    work->attackBody.radius = MAD_CHASER_PAIR_RADIUS;
    work->attackBody.flags  = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, &work->attackBody);
    worldCollisionInitContacts(work->attackContacts, ARRAY_SIZE(work->attackContacts), 0);
    work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);

    MAD_CHASER_SET_SPHERE_ORIGIN(task, &work->gridBody, work->contacts);
    work->gridBody.key    = WORLD_COLLISION_CONTACT_ENEMY_BODY | MAD_CHASER_BODY_ID;
    work->gridBody.radius = MAD_CHASER_GRID_RADIUS;
    work->gridBody.flags  = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, &work->gridBody);
    work->gridBody.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
}

#undef MAD_CHASER_SET_SPHERE_ORIGIN
