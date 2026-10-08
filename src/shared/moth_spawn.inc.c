/* Part of the Moth library; see moth.h. */

/// Allocates and initializes a moth, then enters its living state.
///
/// The task must own a four-part TMD model and carry this enemy in spawnArg2.
/// Work survives until task teardown; allocation failure destroys both task
/// and enemy. Builds both texture-buffer halves after moving texture/CLUT
/// offsets by one, binds lighting and animation slots 1..3, records the home
/// pose and acquires a battle reference. Links radius-250 hit/grid spheres and
/// a radius-400 attack sphere; only the hit and grid tests start enabled.
static void _mothSpawn(Enemy* enemy, Task* task)
{
    enum {
        MOTH_BODY_ID             = 8,
        MOTH_BODY_RADIUS         = 250,
        MOTH_ATTACK_RADIUS       = 400,
        MOTH_HIT_EFFECT_ARGUMENT = 0x100,
        MOTH_HIT_EFFECT_COUNT    = 1
    };

    MothWork*  work;
    GfxCoord*  rootCoord;
    TmdObject* model;
    s32        slotIndex;

    model     = task->extra.tmd;
    rootCoord = model->coords;
    work      = memCalloc(sizeof(MothWork), false);
    if (work == NULL) {
        enemyDestroy(enemy, task);
        return;
    }
    task->work                = work;
    model->flags              = 0;
    rootCoord->composeStamp   = GRAPHICS_COORD_DIRTY;
    model->texturePageOffset += 1;
    model->clutRowOffset     += 1;
    tmdBuildBufferHalf(model);
    tmdBuildBufferHalf(model);
    // The task owns the work; the model and spheres borrow storage inside it.
    model->lightMtx = &work->lightMtx;
    model->colorMtx = &work->colorMtx;
    enemy->field_4  = &rootCoord->coord;
    enemy->field_48 = 0;
    worldTargetLinkNode(&enemy->node);
    enemy->coord                  = rootCoord;
    enemy->node.state.parts.flags = 0;
    enemy->bodyPos.vx             = 0;
    enemy->bodyPos.vy             = 0;
    enemy->bodyPos.vz             = 0;
    enemy->param                  = &gMothParams;
    enemy->recs                   = work->hitContacts;
    enemy->hp                     = (u16)gMothParams.hpMax;
    work->hitEffectArg.spawnArgLo = MOTH_HIT_EFFECT_ARGUMENT;
    work->hitEffectArg.spawnArgHi = MOTH_HIT_EFFECT_COUNT;
    work->hitEffectArg.coord      = rootCoord;
    animationInitContext(&work->rig.anim, gMothAnimSets, model, work->rig.poses, work->rig.slots);
    for (slotIndex = 1; slotIndex < (s32)ARRAY_SIZE(work->rig.slots); slotIndex++) {
        animationResetSlot(&work->rig.anim, slotIndex, 1);
    }
    sceneAcquireBattleRef(0);
    work->flapSign                 = 1;
    work->homePos.vx               = rootCoord->coord.t[0];
    work->homePos.vy               = rootCoord->coord.t[1];
    work->homePos.vz               = rootCoord->coord.t[2];
    work->yaw                      = ((Enemy*)task->spawnArg2.pointer)->place->yaw;
    work->hitBody.coord            = rootCoord;
    work->hitBody.context.contacts = work->hitContacts;
    work->hitBody.pos.vx           = 0;
    work->hitBody.pos.vy           = 0;
    work->hitBody.pos.vz           = 0;
    work->hitBody.key              = (WORLD_COLLISION_CONTACT_ENEMY_BODY | MOTH_BODY_ID);
    work->hitBody.radius           = MOTH_BODY_RADIUS;
    work->hitBody.flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, &work->hitBody);
    worldCollisionInitContacts(work->hitContacts, ARRAY_SIZE(work->hitContacts), 0);
    work->gridBody.coord            = rootCoord;
    work->gridBody.context.contacts = work->gridContacts;
    work->gridBody.pos.vx           = 0;
    work->gridBody.pos.vy           = 0;
    work->gridBody.pos.vz           = 0;
    work->gridBody.key              = (WORLD_COLLISION_CONTACT_ENEMY_BODY | MOTH_BODY_ID);
    work->gridBody.radius           = MOTH_BODY_RADIUS;
    work->gridBody.flags            = WORLD_COLLISION_BODY_SPHERE;
    work->hitBody.flags             = (u16)(work->hitBody.flags | WORLD_COLLISION_BODY_PAIR_ENABLED);
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, &work->gridBody);
    worldCollisionInitContacts(work->gridContacts, ARRAY_SIZE(work->gridContacts), 0);
    work->attackBody.coord            = rootCoord;
    work->attackBody.context.contacts = work->attackContacts;
    work->attackBody.pos.vx           = 0;
    work->attackBody.pos.vy           = 0;
    work->attackBody.pos.vz           = 0;
    work->gridBody.flags              = (u16)(work->gridBody.flags | WORLD_COLLISION_BODY_GRID_ENABLED);
    work->attackBody.key              = damagePackAttackKey(&gMothAttack, 0);
    work->attackBody.radius           = MOTH_ATTACK_RADIUS;
    work->attackBody.flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_ATTACKS, &work->attackBody);
    worldCollisionInitContacts(work->attackContacts, ARRAY_SIZE(work->attackContacts), 0);
    work->attackBody.flags = (u16)(work->attackBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED));
    task->state            = MOTH_TASK_UPDATE;
}
