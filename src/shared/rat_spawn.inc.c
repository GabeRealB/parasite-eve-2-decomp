/* Part of the Rat library; see rat.h. */

/// The first body's set-up handler: allocate the work block, rebind the
/// model's light and colour matrices into it, then link the four collision
/// nodes and their tables. `&obj->coords[4]` -- the model's fifth
/// coordinate -- is what both the context and the second node hang off.
/// A failed allocation tears the enemy down and leaves the task here.
void ratSpawn(Enemy* ctx, Task* actor)
{
    RatWork*   work;
    TmdObject* obj;
    GfxCoord*  coord;
    s32        i;

    obj   = actor->extra.tmd;
    coord = obj->coords;
    work  = memCalloc(sizeof(RatWork), false);
    if (work == NULL) {
        enemyDestroy(ctx, actor);
        return;
    }
    actor->work         = work;
    obj->flags          = 0;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    obj->lightMtx       = &work->lightMtx;
    obj->colorMtx       = &work->colorMtx;
    ctx->field_4        = &coord->coord;
    ctx->field_48       = 0;
    worldTargetLinkNode(&ctx->node);
    ctx->coord                    = &actor->extra.tmd->coords[4];
    ctx->node.state.parts.flags   = 0;
    ctx->bodyPos.vx               = 0;
    ctx->bodyPos.vy               = 0;
    ctx->bodyPos.vz               = 0;
    ctx->param                    = &gRatParams;
    ctx->recs                     = work->hitContacts;
    ctx->hp                       = (u16)gRatParams.hpMax;
    work->hitEffectArg.spawnArgLo = 0x100;
    work->hitEffectArg.spawnArgHi = 1;
    work->hitEffectArg.coord      = coord;
    animationInitContext(&work->rig.anim, gRatAnimSets, obj, work->rig.poses, work->rig.slots);
    for (i = 1; i < 7; i++) {
        animationResetSlot(&work->rig.anim, i, RAT_ANIM_IDLE);
    }
    sceneAcquireBattleRef(0);
    work->animId                      = RAT_ANIM_IDLE;
    work->appliedAnimId               = RAT_ANIM_IDLE;
    work->sensorBody.coord            = coord;
    work->sensorBody.context.contacts = work->sensorContacts;
    work->sensorBody.pos.vx           = 0;
    work->sensorBody.pos.vy           = 0;
    work->sensorBody.pos.vz           = 0x2EE;
    work->sensorBody.key              = 0;
    work->sensorBody.radius           = 0x12C;
    work->sensorBody.flags            = (u32)WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_ATTACKS, &work->sensorBody);
    worldCollisionInitContacts(work->sensorContacts, ARRAY_SIZE(work->sensorContacts), 0);
    work->sensorBody.flags         = (u16)(work->sensorBody.flags | WORLD_COLLISION_BODY_PAIR_ENABLED);
    work->hitBody.coord            = &actor->extra.tmd->coords[4];
    work->hitBody.context.contacts = work->hitContacts;
    work->hitBody.pos.vx           = 0;
    work->hitBody.pos.vy           = 0;
    work->hitBody.pos.vz           = 0;
    work->hitBody.key              = 0x30007;
    work->hitBody.radius           = 0x96;
    work->hitBody.flags            = (u32)WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, &work->hitBody);
    worldCollisionInitContacts(work->hitContacts, ARRAY_SIZE(work->hitContacts), 0);
    work->gridBody.coord            = coord;
    work->gridBody.context.contacts = work->gridContacts;
    work->gridBody.pos.vx           = 0;
    work->gridBody.pos.vy           = -0xFA;
    work->gridBody.pos.vz           = 0;
    work->hitBody.flags             = (u16)(work->hitBody.flags | WORLD_COLLISION_BODY_PAIR_ENABLED);
    work->gridBody.key              = 0x30007;
    work->gridBody.radius           = 0xFA;
    work->gridBody.flags            = (u32)WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, &work->gridBody);
    worldCollisionInitContacts(work->gridContacts, ARRAY_SIZE(work->gridContacts), 0);
    work->attackBody.coord            = coord;
    work->attackBody.context.contacts = work->attackContacts;
    work->attackBody.pos.vx           = 0;
    work->attackBody.pos.vy           = 0;
    work->attackBody.pos.vz           = 0x1F4;
    work->gridBody.flags              = (u16)(work->gridBody.flags | (WORLD_COLLISION_BODY_FLOOR_QUERY | WORLD_COLLISION_BODY_GRID_ENABLED));
    work->attackBody.key              = Gp_PackPair(&gRatAttack, 0);
    work->attackBody.radius           = 0xC8;
    work->attackBody.flags            = (u32)WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_ATTACKS, &work->attackBody);
    worldCollisionInitContacts(work->attackContacts, ARRAY_SIZE(work->attackContacts), 0);
    work->attackBody.flags = (u16)(work->attackBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED));
    actor->state           = 1;
}
