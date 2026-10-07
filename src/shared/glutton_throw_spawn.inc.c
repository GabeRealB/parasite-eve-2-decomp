/* Part of the Glutton library; see glutton.h. */

/// Spawn state of the enemy dispatched through `D_actor_444000_80131EA8`:
/// allocate its work block and stand the model up where the host's first
/// escort is, in world space.
///
/// The model is reparented to `gGfxViewCoord`, so both halves of that escort's
/// part 1 have to be resolved by hand: `_actorRenderAccumulateWorldRotation` walks
/// the part's coordinate chain up to the view coordinate for the rotation and
/// `_actorRenderTransformLocalPointToWorld` carries its origin along the same
/// chain for the translation. The model is then turned a quarter turn, its
/// single display node is linked with a 0x394 extent, and that node is paired with the owning
/// enemy so collisions against it reach this task.
///
/// Bails out -- destroying the enemy -- when the overlay is shutting down, the
/// host actor has left the grab states, or the work block cannot be allocated.
void gluttonThrowSpawn(Enemy* enemy, Task* task)
{
    GluttonProjectileWork* work;
    Enemy*                 owner;
    GluttonWork*           host;
    SVECTOR                pos;
    SVECTOR                vec;

    owner = task->parent->spawnArg2.pointer;
    host  = owner->task->work;

    if (gGluttonEnded == 1 || host->state == 0x10 || host->state == 5 ||
        host->state == 0xC || host->state == 0x12 ||
        (work = memCalloc(sizeof(GluttonProjectileWork), false), task->work = work, work == NULL)) {
        enemyDestroy(enemy, task);
        return;
    }

    work->stateTicks                = 0;
    task->extra.tmd->coords->parent = &gGfxViewCoord;
    task->extra.tmd->flags          = 0;

    _actorRenderAccumulateWorldRotation(&host->escorts[0]->task->extra.tmd->coords[1],
                                        &task->extra.tmd->coords->coord);

    vec.vx = vec.vy = vec.vz = 0;
    _actorRenderTransformLocalPointToWorld(&host->escorts[0]->task->extra.tmd->coords[1], &vec);

    task->extra.tmd->coords->coord.t[0]   = vec.vx;
    task->extra.tmd->coords->coord.t[1]   = vec.vy;
    task->extra.tmd->coords->coord.t[2]   = vec.vz;
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;

    gfxRotMatrixY(&task->extra.tmd->coords->coord, 0x80, 0);
    actorRenderComposeCoord(task->extra.tmd->coords);

    pos.vx = pos.vy = pos.vz = 0;
    _worldCollisionLinkSphereBody(task->extra.tmd->coords, &work->attackBody, work->attackContacts, &pos, 0x394, WORLD_COLLISION_LIST_ENEMY_ATTACKS,
                                  ARRAY_SIZE(work->attackContacts));

    work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    work->attackBody.key    = damagePackEnemyAttackKey(owner, 2);
    work->stateChanged      = 1;
    task->state++;
}
