/* Part of the Rat library; see rat.h. */

/// Fifth model coordinate used by both the target node and hit sphere.
enum { RAT_HIT_COORD_INDEX = 4 };

/// Links and arms the keyless sphere that detects a player ahead of the rat.
///
/// Requires unlinked sensor storage in live work and the model root. Clears
/// the complete sensor contact array before enabling pairing; work and root
/// must remain live until the sensor is unlinked. Uses local coordinate units.
static __inline__ void _ratInitSensorBody(RatWork* work, GfxCoord* rootCoord)
{
    enum {
        RAT_SENSOR_FORWARD_OFFSET = 750,
        RAT_SENSOR_RADIUS         = 300
    };

    work->sensorBody.coord            = rootCoord;
    work->sensorBody.context.contacts = work->sensorContacts;
    work->sensorBody.pos.vx           = 0;
    work->sensorBody.pos.vy           = 0;
    work->sensorBody.pos.vz           = RAT_SENSOR_FORWARD_OFFSET;
    work->sensorBody.key              = 0;
    work->sensorBody.radius           = RAT_SENSOR_RADIUS;
    work->sensorBody.flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_ATTACKS, &work->sensorBody);
    worldCollisionInitContacts(work->sensorContacts, ARRAY_SIZE(work->sensorContacts), 0);
    work->sensorBody.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
}

/// Links the rat's sensor, hit, grid and bite spheres with their contact tables.
///
/// Requires unlinked bodies in zeroed work and a live seven-coordinate model;
/// the supplied root is that model's coordinate 0. Borrows work/model storage,
/// which must outlive all links. Centres and radii use local game-coordinate
/// units. Each contact array is cleared completely and its last entry marked.
/// The sensor and bite use the enemy-attack list; hit/grid use enemy bodies.
/// Preserve flag-enabling order across contact initialization: sensor/hit
/// enable pairing, the grid enables floor/grid queries, and the bite is disabled.
static __inline__ void _ratInitCollisionBodies(RatWork* work, const Task* actor, GfxCoord* rootCoord)
{
    enum {
        RAT_COLLISION_BODY_ID     = 7,
        RAT_COLLISION_BODY_KEY    = WORLD_COLLISION_CONTACT_ENEMY_BODY | RAT_COLLISION_BODY_ID,
        RAT_HIT_RADIUS            = 150,
        RAT_GRID_VERTICAL_OFFSET  = -250,
        RAT_GRID_RADIUS           = 250,
        RAT_ATTACK_FORWARD_OFFSET = 500,
        RAT_ATTACK_RADIUS         = 200
    };

    _ratInitSensorBody(work, rootCoord);
    work->hitBody.coord            = &actor->extra.tmd->coords[RAT_HIT_COORD_INDEX];
    work->hitBody.context.contacts = work->hitContacts;
    work->hitBody.pos.vx           = 0;
    work->hitBody.pos.vy           = 0;
    work->hitBody.pos.vz           = 0;
    work->hitBody.key              = RAT_COLLISION_BODY_KEY;
    work->hitBody.radius           = RAT_HIT_RADIUS;
    work->hitBody.flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, &work->hitBody);
    worldCollisionInitContacts(work->hitContacts, ARRAY_SIZE(work->hitContacts), 0);
    work->gridBody.coord            = rootCoord;
    work->gridBody.context.contacts = work->gridContacts;
    work->gridBody.pos.vx           = 0;
    work->gridBody.pos.vy           = RAT_GRID_VERTICAL_OFFSET;
    work->gridBody.pos.vz           = 0;
    work->hitBody.flags            |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    work->gridBody.key              = RAT_COLLISION_BODY_KEY;
    work->gridBody.radius           = RAT_GRID_RADIUS;
    work->gridBody.flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, &work->gridBody);
    worldCollisionInitContacts(work->gridContacts, ARRAY_SIZE(work->gridContacts), 0);
    work->attackBody.coord            = rootCoord;
    work->attackBody.context.contacts = work->attackContacts;
    work->attackBody.pos.vx           = 0;
    work->attackBody.pos.vy           = 0;
    work->attackBody.pos.vz           = RAT_ATTACK_FORWARD_OFFSET;
    work->gridBody.flags             |= (WORLD_COLLISION_BODY_FLOOR_QUERY | WORLD_COLLISION_BODY_GRID_ENABLED);
    work->attackBody.key              = damagePackAttackKey(&gRatAttack, 0);
    work->attackBody.radius           = RAT_ATTACK_RADIUS;
    work->attackBody.flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_ATTACKS, &work->attackBody);
    worldCollisionInitContacts(work->attackContacts, ARRAY_SIZE(work->attackContacts), 0);
    work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
}

void ratSpawn(Enemy* enemy, Task* actor)
{
    RatWork*   work;
    TmdObject* model;
    GfxCoord*  rootCoord;
    s32        slot;

    model     = actor->extra.tmd;
    rootCoord = model->coords;
    work      = memCalloc(sizeof(RatWork), false);
    if (work == NULL) {
        enemyDestroy(enemy, actor);
        return;
    }
    // Lighting and collision readers borrow storage owned by the task.
    actor->work             = work;
    model->flags            = 0;
    rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    model->lightMtx         = &work->lightMtx;
    model->colorMtx         = &work->colorMtx;
    enemy->field_4          = &rootCoord->coord;
    enemy->field_48         = 0;
    worldTargetLinkNode(&enemy->node);
    enemy->coord                  = &actor->extra.tmd->coords[RAT_HIT_COORD_INDEX];
    enemy->node.state.parts.flags = 0;
    enemy->bodyPos.vx             = 0;
    enemy->bodyPos.vy             = 0;
    enemy->bodyPos.vz             = 0;
    enemy->param                  = &gRatParams;
    enemy->recs                   = work->hitContacts;
    enemy->hp                     = gRatParams.hpMax;
    work->hitEffectArg.spawnArgLo = 0x100;
    work->hitEffectArg.spawnArgHi = 1;
    work->hitEffectArg.coord      = rootCoord;
    // Slot zero is the root; reset only the six articulated-part tracks.
    animationInitContext(&work->rig.anim, gRatAnimSets, model, work->rig.poses, work->rig.slots);
    for (slot = 1; slot < ARRAY_SIZE(work->rig.slots); slot++) {
        animationResetSlot(&work->rig.anim, slot, RAT_ANIM_IDLE);
    }
    sceneAcquireBattleRef(0);
    work->animId        = RAT_ANIM_IDLE;
    work->appliedAnimId = RAT_ANIM_IDLE;
    _ratInitCollisionBodies(work, actor, rootCoord);
    actor->state = RAT_TASK_UPDATE;
}
