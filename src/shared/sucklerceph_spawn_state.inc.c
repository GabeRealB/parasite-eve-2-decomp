/* Part of the Sucklerceph library; see sucklerceph.h. */

/// Initializes the standing Sucklerceph's model, animation and collision state.
///
/// Called in task state 0 with the owning enemy and a three-part TMD model.
/// A signed high spawn halfword of 1 cancels the spawn; allocation failure
/// likewise destroys the enemy and task. On success the task owns zeroed
/// `SucklercephWork`, the enemy borrows its body contacts and part-1 coordinate,
/// and `_sucklercephExit` owns detachment before common teardown.
///
/// The low spawn halfword selects the sound variant; value 1 on a TMD body
/// also shifts the texture page and CLUT row and rebuilds both buffer halves.
/// Starts idle tracks 1 and 2, enables sensing and ordinary body collision,
/// leaves the two burst bodies inactive, acquires one battle hold and advances
/// to the active task state. The stored high halfword's further role is unproven.
static void _sucklercephSpawnState(Enemy* enemy, Task* task)
{
    enum {
        SUCKLERCEPH_SPAWN_CANCELLED   = 1,
        SUCKLERCEPH_TEXTURE_VARIANT   = 1,
        SUCKLERCEPH_SENSE_RADIUS      = 3000,
        SUCKLERCEPH_BODY_RADIUS       = 200,
        SUCKLERCEPH_BURST_RADIUS      = 1000,
        SUCKLERCEPH_BODY_KEY          = WORLD_COLLISION_CONTACT_ENEMY_BODY | 0x2E,
        SUCKLERCEPH_BLAST_HIT_KEY     = 0x22323,
        SUCKLERCEPH_HIT_EFFECT_ARG_LO = 256,
        SUCKLERCEPH_HIT_EFFECT_ARG_HI = 1
    };

    SucklercephWork* work;
    TmdObject*       model;
    GfxCoord*        rootCoord;
    GfxCoord*        bodyCoord;
    u16              variant;
    s32              slotIndex;

    model     = task->extra.tmd;
    rootCoord = model->coords;
    bodyCoord = &rootCoord[1];
    if ((s16)(task->spawnArg1.value >> 16) == SUCKLERCEPH_SPAWN_CANCELLED) {
        enemyDestroy(enemy, task);
        return;
    }
    work = memCalloc(sizeof(SucklercephWork), false);
    if (work == NULL) {
        enemyDestroy(enemy, task);
        return;
    }
    // The model and enemy borrow matrices, part 1 and contact storage from the work.
    task->work              = work;
    model->flags            = 0;
    rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    model->lightMtx         = &work->lightMtx;
    model->colorMtx         = &work->colorMtx;
    enemy->field_4          = &rootCoord[1].coord;
    enemy->field_48         = 0;
    worldTargetLinkNode(&enemy->node);
    enemy->coord                  = bodyCoord;
    enemy->node.state.parts.flags = 0;
    enemy->bodyPos.vx             = 0;
    enemy->bodyPos.vy             = 0;
    enemy->bodyPos.vz             = 0;
    enemy->param                  = &gSucklercephParams;
    enemy->recs                   = work->contacts;
    enemy->hp                     = gSucklercephParams.hpMax;
    animationInitContext(&work->anim, gSucklercephAnimSets, model, work->poses, work->slots);
    for (slotIndex = 1; slotIndex < ARRAY_SIZE(work->slots); slotIndex++) {
        animationResetSlot(&work->anim, slotIndex, SUCKLERCEPH_ANIM_IDLE);
    }
    sceneAcquireBattleRef(0);
    work->animId                  = SUCKLERCEPH_ANIM_IDLE;
    work->appliedAnim             = SUCKLERCEPH_ANIM_IDLE;
    work->swellScale              = ONE;
    work->hasBurst                = 0;
    work->hitCooldown             = 0;
    work->swellFrames             = 0;
    work->animFrozen              = 0;
    work->field_2CC               = 0;
    task->killCountdown           = 0;
    work->hitEffectArg.coord      = &task->extra.tmd->coords[1];
    work->hitEffectArg.spawnArgLo = SUCKLERCEPH_HIT_EFFECT_ARG_LO;
    work->hitEffectArg.spawnArgHi = SUCKLERCEPH_HIT_EFFECT_ARG_HI;
    // Sensing and ordinary contacts are live; the burst bodies wait.
    // Shape setup only. member names a sphere field; pointers must be stable and
    // side-effect-free. Scalars are evaluated once. This is a statement sequence:
    // use only as a standalone statement in a braced body, never as an expression.
    // Linking and pass enables follow separately.
#define SUCKLERCEPH_INIT_SPHERE(work, member, root, contactTable, centreY, sphereRadius, contactKey) \
    (work)->member.coord            = (root);                                                        \
    (work)->member.context.contacts = (contactTable);                                                \
    (work)->member.pos.vx           = 0;                                                             \
    (work)->member.pos.vy           = (centreY);                                                     \
    (work)->member.pos.vz           = 0;                                                             \
    (work)->member.key              = (contactKey);                                                  \
    (work)->member.radius           = (sphereRadius);                                                \
    (work)->member.flags            = WORLD_COLLISION_BODY_SPHERE
    SUCKLERCEPH_INIT_SPHERE(work, senseBody, rootCoord, &work->senseContact, 0, SUCKLERCEPH_SENSE_RADIUS, 0);
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_ATTACKS, &work->senseBody);
    worldCollisionInitContacts(&work->senseContact, 1, 0);
    SUCKLERCEPH_INIT_SPHERE(work, body, rootCoord, work->contacts, -SUCKLERCEPH_BODY_RADIUS, SUCKLERCEPH_BODY_RADIUS, SUCKLERCEPH_BODY_KEY);
    work->senseBody.flags = (u16)(work->senseBody.flags | WORLD_COLLISION_BODY_PAIR_ENABLED);
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, &work->body);
    worldCollisionInitContacts(work->contacts, ARRAY_SIZE(work->contacts), 0);
    work->attackBody.coord            = rootCoord;
    work->attackBody.context.contacts = &work->attackContact;
    work->attackBody.pos.vx           = 0;
    work->attackBody.pos.vy           = 0;
    work->attackBody.pos.vz           = 0;
    work->body.flags                  = (u16)(work->body.flags | (WORLD_COLLISION_BODY_FLOOR_QUERY | WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
    work->attackBody.key              = damagePackAttackKey(&gSucklercephAttack, 0);
    work->attackBody.radius           = SUCKLERCEPH_BURST_RADIUS;
    work->attackBody.flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_ATTACKS, &work->attackBody);
    worldCollisionInitContacts(&work->attackContact, 1, 0);
    SUCKLERCEPH_INIT_SPHERE(work, blastBody, rootCoord, &work->blastContact, 0, SUCKLERCEPH_BURST_RADIUS, SUCKLERCEPH_BLAST_HIT_KEY);
    work->attackBody.flags = (u16)(work->attackBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED));
    worldCollisionLinkBody(WORLD_COLLISION_LIST_BLASTS, &work->blastBody);
    worldCollisionInitContacts(&work->blastContact, 1, 0);
#undef SUCKLERCEPH_INIT_SPHERE

    work->blastBody.flags = (u16)(work->blastBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED));
    work->field_2DC       = task->spawnArg1.value >> 16;
    variant               = (u16)task->spawnArg1.value;
    work->variant         = variant;
    if ((s16)variant == SUCKLERCEPH_TEXTURE_VARIANT && task->bodyKind == (s16)variant) {
        model->texturePageOffset = model->texturePageOffset + 1;
        model->clutRowOffset     = model->clutRowOffset + 1;
        if (model->buffer != 0) {
            tmdBuildBufferHalf(model);
            tmdBuildBufferHalf(model);
        }
    }
    work->deathPhase   = SUCKLERCEPH_DEATH_PHASE_COUNTDOWN;
    task->exitCallback = _sucklercephExit;
    task->state       += 1;
}
