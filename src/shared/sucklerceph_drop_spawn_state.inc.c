/* Part of the Sucklerceph library; see sucklerceph.h. */

/// Initializes a hidden Sucklerceph that waits for a room command to begin dropping.
///
/// Requires the owning enemy and a live three-part TMD model. A signed high
/// spawn halfword of 1 or work-allocation failure destroys the enemy and task.
/// The low halfword selects the sound variant; the retained high halfword's
/// further role is unproven. On success the task owns zeroed work, acquires a
/// battle hold and enters drop task state 3 with idle tracks 1 and 2.
/// The model borrows the work's matrices; the enemy borrows part 1 and contacts.
/// All four spheres are linked with pair tests disabled, and the ordinary body
/// also has floor/grid queries disabled. The room message table later reveals
/// and arms the drop. Common task teardown releases the work.
static void _sucklercephDropSpawnState(Enemy* enemy, Task* task)
{
    enum {
        SUCKLERCEPH_DROP_SPAWN_CANCELLED   = 1,
        SUCKLERCEPH_DROP_SENSE_RADIUS      = 3000,
        SUCKLERCEPH_DROP_BODY_RADIUS       = 200,
        SUCKLERCEPH_DROP_BURST_RADIUS      = 1000,
        SUCKLERCEPH_DROP_BODY_KEY          = WORLD_COLLISION_CONTACT_ENEMY_BODY | 0x2E,
        SUCKLERCEPH_DROP_BLAST_KEY         = 0x22323,
        SUCKLERCEPH_DROP_HIT_EFFECT_ARG_LO = 256,
        SUCKLERCEPH_DROP_HIT_EFFECT_ARG_HI = 1,
        SUCKLERCEPH_DROP_TASK_WAIT         = 3
    };
    SucklercephWork* work;
    GfxCoord*        rootCoord;
    GfxCoord*        bodyCoord;
    TmdObject*       model;
    s32              slotIndex;

    model     = task->extra.tmd;
    rootCoord = model->coords;
    bodyCoord = &rootCoord[1];
    if ((s16)(task->spawnArg1.value >> 16) == SUCKLERCEPH_DROP_SPAWN_CANCELLED) {
        enemyDestroy(enemy, task);
        return;
    }
    work = memCalloc(sizeof(SucklercephWork), false);
    if (work == NULL) {
        enemyDestroy(enemy, task);
        return;
    }
    // Attach work before publishing the pointers the model and enemy borrow.
    task->work              = work;
    work->field_2DC         = task->spawnArg1.value >> 16;
    work->variant           = task->spawnArg1.value;
    model->flags            = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    model->lightMtx         = &work->lightMtx;
    model->colorMtx         = &work->colorMtx;
    enemy->field_4          = &rootCoord[1].coord;
    enemy->field_48         = 0;
    worldTargetLinkNode(&enemy->node);
    enemy->coord                  = bodyCoord;
    enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
    enemy->bodyPos.vx             = 0;
    enemy->bodyPos.vy             = 0;
    enemy->bodyPos.vz             = 0;
    enemy->param                  = &gSucklercephParams;
    enemy->recs                   = work->contacts;
    enemy->hp                     = gSucklercephParams.hpMax;
    animationInitContext(&work->anim, gSucklercephAnimSets, model, work->poses, work->slots);
    slotIndex = 1;
    do {
        animationResetSlot(&work->anim, slotIndex, SUCKLERCEPH_ANIM_IDLE);
        slotIndex += 1;
    } while (slotIndex < ARRAY_SIZE(work->slots));
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
    work->hitEffectArg.spawnArgLo = SUCKLERCEPH_DROP_HIT_EFFECT_ARG_LO;
    work->hitEffectArg.spawnArgHi = SUCKLERCEPH_DROP_HIT_EFFECT_ARG_HI;
    // Link sensing, body and burst spheres without enabling their passes.
    // Shape setup only. work is evaluated eight times; other value arguments
    // once. Supply stable, side-effect-free pointers; member selects a sphere
    // field. Expands statements, so use only standalone
    // in a braced body. Linking and pass flags remain in their original order.
#define SUCKLERCEPH_INIT_DROP_SPHERE(work, member, root, contactTable, centreY, sphereRadius, contactKey) \
    (work)->member.coord            = (root);                                                             \
    (work)->member.context.contacts = (contactTable);                                                     \
    (work)->member.pos.vx           = 0;                                                                  \
    (work)->member.pos.vy           = (centreY);                                                          \
    (work)->member.pos.vz           = 0;                                                                  \
    (work)->member.key              = (contactKey);                                                       \
    (work)->member.radius           = (sphereRadius);                                                     \
    (work)->member.flags            = WORLD_COLLISION_BODY_SPHERE
    SUCKLERCEPH_INIT_DROP_SPHERE(work, senseBody, rootCoord, &work->senseContact, 0, SUCKLERCEPH_DROP_SENSE_RADIUS, 0);
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_ATTACKS, &work->senseBody);
    worldCollisionInitContacts(&work->senseContact, 1, 0);
    SUCKLERCEPH_INIT_DROP_SPHERE(work, body, rootCoord, work->contacts, -SUCKLERCEPH_DROP_BODY_RADIUS, SUCKLERCEPH_DROP_BODY_RADIUS, SUCKLERCEPH_DROP_BODY_KEY);
    work->senseBody.flags = (u16)(work->senseBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED));
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, &work->body);
    worldCollisionInitContacts(work->contacts, ARRAY_SIZE(work->contacts), 0);
    work->attackBody.coord            = rootCoord;
    work->attackBody.context.contacts = &work->attackContact;
    work->attackBody.pos.vx           = 0;
    work->attackBody.pos.vy           = 0;
    work->attackBody.pos.vz           = 0;
    work->body.flags                  = (u16)(work->body.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_FLOOR_QUERY | WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED)));
    work->attackBody.key              = damagePackAttackKey(&gSucklercephAttack, 0);
    work->attackBody.radius           = SUCKLERCEPH_DROP_BURST_RADIUS;
    work->attackBody.flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_ATTACKS, &work->attackBody);
    worldCollisionInitContacts(&work->attackContact, 1, 0);
    SUCKLERCEPH_INIT_DROP_SPHERE(work, blastBody, rootCoord, &work->blastContact, 0, SUCKLERCEPH_DROP_BURST_RADIUS, SUCKLERCEPH_DROP_BLAST_KEY);
    work->attackBody.flags = (u16)(work->attackBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED));
    worldCollisionLinkBody(WORLD_COLLISION_LIST_BLASTS, &work->blastBody);
    worldCollisionInitContacts(&work->blastContact, 1, 0);
#undef SUCKLERCEPH_INIT_DROP_SPHERE

    work->dropArmed       = 0;
    work->blastBody.flags = (u16)(work->blastBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED));
    task->msgTable        = gSucklercephDropMsgTable;
    task->state           = SUCKLERCEPH_DROP_TASK_WAIT;
}
