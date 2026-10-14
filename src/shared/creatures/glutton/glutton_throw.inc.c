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

    if (_gGluttonEncounterEnded == GLUTTON_ENCOUNTER_ENDED || hostWork->state == 0x10 || hostWork->state == 5 ||
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

/// Composes the ground-plane coordinate beneath the thrown sphere.
///
/// Function-local binding: work and task must be side-effect-free pointers to
/// live projectile work and its coordinate-body task. Arguments are evaluated
/// repeatedly. Invoke as a standalone statement; undefined after this function.
#define GLUTTON_UPDATE_THROW_SHADOW(work, task)                                        \
    {                                                                                  \
        (work)->shadowCoord.parent = &gGfxViewCoord;                                   \
        gfxRotMatrixY(&(work)->shadowCoord.coord, 0, GRAPHICS_ROTATION_REPLACE);       \
        (work)->shadowCoord.coord.t[0]   = (task)->extra.coordBody->coord->coord.t[0]; \
        (work)->shadowCoord.coord.t[1]   = 0;                                          \
        (work)->shadowCoord.coord.t[2]   = (task)->extra.coordBody->coord->coord.t[2]; \
        (work)->shadowCoord.composeStamp = GRAPHICS_COORD_DIRTY;                       \
        actorRenderComposeCoord(&(work)->shadowCoord);                                 \
    }

/// Moves the Glutton's thrown attack sphere forward and draws its ground shadow.
///
/// Requires a coordinate-body task, projectile work, and a live parent whose
/// enemy points to the host. Running ticks add 10 to Y and move 137
/// parent-coordinate units along the normalized local Z axis. From tick 41,
/// Y is held at -1000; tick 53 unlinks the attack sphere and advances the state. Entry arms
/// pair testing and resets the shadow. Paused ticks only refresh the shadow,
/// omitting its draw in host phase 6. Fight end and host states 5, 12, 16 or 18
/// unlink the attack body and advance without releasing the task here.
///
/// Requires initialized scratch with one SVECTOR plus nested shadow-draw
/// capacity. The reservation is released before return; GTE state is overwritten.
static void _gluttonThrowFly(Enemy* enemy, Task* task)
{
    enum { GLUTTON_THROW_FORWARD_STEP          = 137,
           GLUTTON_THROW_FALL_STEP             = 10,
           GLUTTON_THROW_HEIGHT_LOCK_TICK      = 41,
           GLUTTON_THROW_END_TICK              = 53,
           GLUTTON_THROW_LOCKED_Y              = -1000,
           GLUTTON_THROW_SHADOW_INITIAL_GROWTH = 0x400,
           GLUTTON_THROW_SHADOW_GROWTH_STEP    = 0x60,
           GLUTTON_THROW_SHADOW_BASE_SIZE      = 0x100,
           GLUTTON_THROW_NO_SHADOW_PHASE       = 6 };
    GluttonProjectileWork* work;
    GluttonWork*           host;
    Enemy*                 owner;
    SVECTOR*               displacement;

    work  = task->work;
    owner = task->parent->spawnArg2.pointer;
    host  = owner->task->work;

    if (_gGluttonEncounterEnded == GLUTTON_ENCOUNTER_ENDED || host->state == 0x10 || host->state == 5 ||
        host->state == 0xC || host->state == 0x12) {
        task->state++;
        worldCollisionUnlinkBody(&work->attackBody);
        return;
    }

    displacement = SCRATCH_STACK_RESERVE_BLOCK(SVECTOR);

    if (work->stateChanged != 0) {
        work->stateTicks                  = 0;
        work->shadowGrowth                = GLUTTON_THROW_SHADOW_INITIAL_GROWTH;
        work->stateChanged                = 0;
        work->attackContacts[0].key.value = 0;
        work->attackBody.flags           |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    }

    if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING) {
        work->stateTicks++;
        task->extra.coordBody->coord->coord.t[1] += GLUTTON_THROW_FALL_STEP;

        // Normalize the launch axis so rotation scale does not change speed.
        gfxReadMatrixZAxis(&task->extra.coordBody->coord->coord, displacement);
        _actorMovementBuildDisplacement(displacement, GLUTTON_THROW_FORWARD_STEP);

        task->extra.coordBody->coord->coord.t[0] += displacement->vx;
        task->extra.coordBody->coord->coord.t[1] += displacement->vy;
        if (work->stateTicks >= GLUTTON_THROW_HEIGHT_LOCK_TICK) {
            task->extra.coordBody->coord->coord.t[1] = GLUTTON_THROW_LOCKED_Y;
        }
        task->extra.coordBody->coord->coord.t[2]  += displacement->vz;
        task->extra.coordBody->coord->composeStamp = GRAPHICS_COORD_DIRTY;

        work->shadowGrowth += GLUTTON_THROW_SHADOW_GROWTH_STEP;
        worldCollisionClearContacts(work->attackContacts);

        GLUTTON_UPDATE_THROW_SHADOW(work, task);

        effectDrawGroundShadow(GRAPHICS_MATRIX_TRANSLATION(&work->shadowCoord.workm), (work->shadowGrowth >> 3) + GLUTTON_THROW_SHADOW_BASE_SIZE,
                               gRoomEffectState->groundShadowShade);

        if (work->stateTicks >= GLUTTON_THROW_END_TICK) {
            worldCollisionUnlinkBody(&work->attackBody);
            task->state++;
            work->stateChanged = 1;
        }
    } else {
        GLUTTON_UPDATE_THROW_SHADOW(work, task);

        if (host->phase != GLUTTON_THROW_NO_SHADOW_PHASE) {
            effectDrawGroundShadow(GRAPHICS_MATRIX_TRANSLATION(&work->shadowCoord.workm), (work->shadowGrowth >> 3) + GLUTTON_THROW_SHADOW_BASE_SIZE,
                                   gRoomEffectState->groundShadowShade);
        }
    }

    SCRATCH_STACK_RELEASE_BLOCK(SVECTOR);
}

#undef GLUTTON_UPDATE_THROW_SHADOW
