/* Part of the Glutton library; see glutton.h. */

/// Copies escort 0's part-1 world pose and composes the sphere's local-Y turn.
///
/// Function-local operation: task and hostWork must be side-effect-free live
/// pointers to a coordinate-body task and host work; launchPoint must be a
/// writable SVECTOR lvalue. Arguments are evaluated repeatedly. The caller
/// supplies GLUTTON_THROW_LOCAL_Y_TURN in 4096 units per turn. No pointer is
/// retained; invoke as a standalone statement. Undefined after the function.
#define GLUTTON_PLACE_THROWN_SPHERE(task, hostWork, launchPoint)                                                      \
    {                                                                                                                 \
        _actorRenderAccumulateWorldRotation(&(hostWork)->escorts[0]->task->extra.tmd->coords[1],                      \
                                            &(task)->extra.coordBody->coord->coord);                                  \
                                                                                                                      \
        (launchPoint).vx = (launchPoint).vy = (launchPoint).vz = 0;                                                   \
        _actorRenderTransformLocalPointToWorld(&(hostWork)->escorts[0]->task->extra.tmd->coords[1], &(launchPoint));  \
                                                                                                                      \
        (task)->extra.coordBody->coord->coord.t[0]   = (launchPoint).vx;                                              \
        (task)->extra.coordBody->coord->coord.t[1]   = (launchPoint).vy;                                              \
        (task)->extra.coordBody->coord->coord.t[2]   = (launchPoint).vz;                                              \
        (task)->extra.coordBody->coord->composeStamp = GRAPHICS_COORD_DIRTY;                                          \
                                                                                                                      \
        gfxRotMatrixY(&(task)->extra.coordBody->coord->coord, GLUTTON_THROW_LOCAL_Y_TURN, GRAPHICS_ROTATION_COMPOSE); \
        actorRenderComposeCoord((task)->extra.coordBody->coord);                                                      \
    }

/// Creates a thrown attack sphere at escort 0's part-1 world transform.
///
/// Requires a live coordinate body and parent host with escort 0, its part 1
/// and attack entry 2. Allocates zeroed task-owned projectile work, composes
/// a local-Y turn of 128/4096 revolution and links a radius-916 attack sphere
/// with pair tests disabled until flight entry. Clears only the low half of
/// the coordinate body's `field_C`; that field's meaning remains unproven.
/// Fight end, host states 5, 12, 16 or 18, or allocation failure destroys the
/// enemy; success requests flight entry and advances the task.
static void _gluttonThrowSpawn(Enemy* enemy, Task* task)
{
    enum { GLUTTON_THROW_LOCAL_Y_TURN = ACTOR_TRANSFORM_ANGLE_TURN / 32,
           GLUTTON_THROW_RADIUS       = 916,
           GLUTTON_THROW_ATTACK_INDEX = 2 };
    GluttonProjectileWork* work;
    Enemy*                 hostEnemy;
    GluttonWork*           hostWork;
    SVECTOR                sphereOffset;
    SVECTOR                launchPoint;

    hostEnemy = task->parent->spawnArg2.pointer;
    hostWork  = hostEnemy->task->work;

    if (gGluttonEnded == 1 || hostWork->state == 0x10 || hostWork->state == 5 ||
        hostWork->state == 0xC || hostWork->state == 0x12 ||
        (work = memCalloc(sizeof(GluttonProjectileWork), false), task->work = work, work == NULL)) {
        enemyDestroy(enemy, task);
        return;
    }

    work->stateTicks                     = 0;
    task->extra.coordBody->coord->parent = &gGfxViewCoord;
    // Retain the original halfword clear; the upper half is left intact.
    *(u16*)&task->extra.coordBody->field_C = 0;

    // Detach the launch transform from the escort without losing its world pose.
    GLUTTON_PLACE_THROWN_SPHERE(task, hostWork, launchPoint);

    sphereOffset.vx = sphereOffset.vy = sphereOffset.vz = 0;
    _worldCollisionLinkSphereBody(task->extra.coordBody->coord, &work->attackBody, work->attackContacts, &sphereOffset, GLUTTON_THROW_RADIUS, WORLD_COLLISION_LIST_ENEMY_ATTACKS,
                                  ARRAY_SIZE(work->attackContacts));

    work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    work->attackBody.key    = damagePackEnemyAttackKey(hostEnemy, GLUTTON_THROW_ATTACK_INDEX);
    work->stateChanged      = 1;
    task->state++;
}

#undef GLUTTON_PLACE_THROWN_SPHERE
