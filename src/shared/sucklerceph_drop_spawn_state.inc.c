/* Part of the Sucklerceph library; see sucklerceph.h. */

/// Spawn handler of the dropping first enemy, entry 0 of `Actor04600_D00010`.
/// A spawn arg whose high halfword is 1 destroys the enemy instead. Otherwise
/// it builds the same work block as `sucklercephSpawnState` with the model hidden
/// and the node flag set, keeps the spawn arg's two halves, leaves the first
/// body's 0x8000 bit and the second's 0xC200 bits clear, parks
/// `gSucklercephDropMsgTable` as the task's message table and moves the task to state
/// 3, the drop.
void sucklercephDropSpawnState(Enemy* arg0, Task* arg1)
{
    SucklercephWork* work;
    GfxCoord*        coord;
    GfxCoord*        part;
    TmdObject*       obj;
    s32              one;
    s32              i;

    obj   = arg1->extra.tmd;
    coord = obj->coords;
    part  = &coord[1];
    one   = 1;
    if ((s16)(arg1->spawnArg1.value >> 16) == one) {
        enemyDestroy(arg0, arg1);
        return;
    }
    work = memCalloc(sizeof(SucklercephWork), false);
    if (work == NULL) {
        enemyDestroy(arg0, arg1);
        return;
    }
    arg1->work          = work;
    work->field_2DC     = arg1->spawnArg1.value >> 16;
    work->variant       = arg1->spawnArg1.value;
    obj->flags          = TMD_OBJECT_SKIP_ACTIVE_DRAW;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    obj->lightMtx       = &work->lightMtx;
    obj->colorMtx       = &work->colorMtx;
    arg0->field_4       = &coord[1].coord;
    arg0->field_48      = 0;
    worldTargetLinkNode(&arg0->node);
    arg0->coord                  = part;
    arg0->node.state.parts.flags = one;
    arg0->bodyPos.vx             = 0;
    arg0->bodyPos.vy             = 0;
    arg0->bodyPos.vz             = 0;
    arg0->param                  = &gSucklercephParams;
    arg0->recs                   = work->contacts;
    arg0->hp                     = gSucklercephParams.hpMax;
    animationInitContext(&work->anim, gSucklercephAnimSets, obj, work->poses, work->slots);
    i = 1;
    do {
        animationResetSlot(&work->anim, i, SUCKLERCEPH_ANIM_IDLE);
        i += 1;
    } while (i < ARRAY_SIZE(work->slots));
    (sceneAcquireBattleRef)(0);
    work->animId                     = SUCKLERCEPH_ANIM_IDLE;
    work->appliedAnim                = SUCKLERCEPH_ANIM_IDLE;
    work->swellScale                 = ONE;
    work->hasBurst                   = 0;
    work->hitCooldown                = 0;
    work->swellFrames                = 0;
    work->animFrozen                 = 0;
    work->field_2CC                  = 0;
    arg1->killCountdown              = 0;
    work->hitEffectArg.coord         = &arg1->extra.tmd->coords[1];
    work->hitEffectArg.spawnArgLo    = 0x100;
    work->hitEffectArg.spawnArgHi    = 1;
    work->senseBody.coord            = coord;
    work->senseBody.context.contacts = &work->senseContact;
    work->senseBody.pos.vx           = 0;
    work->senseBody.pos.vy           = 0;
    work->senseBody.pos.vz           = 0;
    work->senseBody.key              = 0;
    work->senseBody.radius           = 0xBB8;
    work->senseBody.flags            = (u32)WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_ATTACKS, &work->senseBody);
    worldCollisionInitContacts(&work->senseContact, 1, 0);
    work->body.coord            = coord;
    work->body.context.contacts = work->contacts;
    work->body.pos.vx           = 0;
    work->body.pos.vy           = -0xC8;
    work->body.pos.vz           = 0;
    work->body.key              = 0x3002E;
    work->body.radius           = 0xC8;
    work->body.flags            = (u32)WORLD_COLLISION_BODY_SPHERE;
    work->senseBody.flags       = (u16)(work->senseBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED));
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, &work->body);
    worldCollisionInitContacts(work->contacts, ARRAY_SIZE(work->contacts), 0);
    work->attackBody.coord            = coord;
    work->attackBody.context.contacts = &work->attackContact;
    work->attackBody.pos.vx           = 0;
    work->attackBody.pos.vy           = 0;
    work->attackBody.pos.vz           = 0;
    work->body.flags                  = (u16)(work->body.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_FLOOR_QUERY | WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED)));
    work->attackBody.key              = Gp_PackPair(&gSucklercephAttack, 0);
    work->attackBody.radius           = 0x3E8;
    work->attackBody.flags            = (u32)WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_ATTACKS, &work->attackBody);
    worldCollisionInitContacts(&work->attackContact, 1, 0);
    work->blastBody.coord            = coord;
    work->blastBody.context.contacts = &work->blastContact;
    work->blastBody.pos.vx           = 0;
    work->blastBody.pos.vy           = 0;
    work->blastBody.pos.vz           = 0;
    work->blastBody.key              = 0x22323;
    work->blastBody.radius           = 0x3E8;
    work->blastBody.flags            = (u32)WORLD_COLLISION_BODY_SPHERE;
    work->attackBody.flags           = (u16)(work->attackBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED));
    worldCollisionLinkBody(WORLD_COLLISION_LIST_BLASTS, &work->blastBody);
    worldCollisionInitContacts(&work->blastContact, 1, 0);
    work->dropArmed       = 0;
    work->blastBody.flags = (u16)(work->blastBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED));
    arg1->msgTable        = gSucklercephDropMsgTable;
    arg1->state           = 3;
}
