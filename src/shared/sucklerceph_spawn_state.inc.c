/* Part of the Sucklerceph library; see sucklerceph.h. */

/// Spawn handler of the first enemy, entry 0 of `Actor04600_D00004`. A spawn
/// arg whose high halfword is 1 destroys the enemy instead. Otherwise it
/// allocates the 0x2E4-byte work block, points the model's light and colour
/// matrices into it, links the enemy's node, seeds the animation context and
/// resets slots 1 and 2, then links the four bodies with their contact tables
/// and installs `sucklercephExit` as the exit callback. The spawn arg's two
/// halves are kept in `field_2DC`/`variant`; a low half of 1 matching the
/// task's `bodyKind` steps the model's texture page and CLUT row and
/// re-streams it twice.
void sucklercephSpawnState(Enemy* arg0, Task* arg1)
{
    SucklercephWork* work;
    TmdObject*       obj;
    GfxCoord*        coord;
    GfxCoord*        part;
    u16              v;
    s32              i;

    obj   = arg1->extra.tmd;
    coord = obj->coords;
    part  = &coord[1];
    if ((s16)(arg1->spawnArg1.value >> 16) == 1) {
        enemyDestroy(arg0, arg1);
        return;
    }
    work = memCalloc(sizeof(SucklercephWork), false);
    if (work == NULL) {
        enemyDestroy(arg0, arg1);
        return;
    }
    arg1->work          = work;
    obj->flags          = 0;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    obj->lightMtx       = &work->lightMtx;
    obj->colorMtx       = &work->colorMtx;
    arg0->field_4       = &coord[1].coord;
    arg0->field_48      = 0;
    Gp_LinkNode(&arg0->node);
    arg0->coord                  = part;
    arg0->node.state.parts.flags = 0;
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
    (Gp_IncStateF0Ref)(0);
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
    Gp_LinkObj(3, &work->senseBody);
    worldCollisionInitContacts(&work->senseContact, 1, 0);
    work->body.coord            = coord;
    work->body.context.contacts = work->contacts;
    work->body.pos.vx           = 0;
    work->body.pos.vy           = -0xC8;
    work->body.pos.vz           = 0;
    work->body.key              = 0x3002E;
    work->body.radius           = 0xC8;
    work->body.flags            = (u32)WORLD_COLLISION_BODY_SPHERE;
    work->senseBody.flags       = (u16)(work->senseBody.flags | WORLD_COLLISION_BODY_PAIR_ENABLED);
    Gp_LinkObj(2, &work->body);
    worldCollisionInitContacts(work->contacts, ARRAY_SIZE(work->contacts), 0);
    work->attackBody.coord            = coord;
    work->attackBody.context.contacts = &work->attackContact;
    work->attackBody.pos.vx           = 0;
    work->attackBody.pos.vy           = 0;
    work->attackBody.pos.vz           = 0;
    work->body.flags                  = (u16)(work->body.flags | (WORLD_COLLISION_BODY_FLOOR_QUERY | WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
    work->attackBody.key              = Gp_PackPair(&gSucklercephAttack, 0);
    work->attackBody.radius           = 0x3E8;
    work->attackBody.flags            = (u32)WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(3, &work->attackBody);
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
    Gp_LinkObj(8, &work->blastBody);
    worldCollisionInitContacts(&work->blastContact, 1, 0);
    work->blastBody.flags = (u16)(work->blastBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED));
    work->field_2DC       = arg1->spawnArg1.value >> 16;
    v                     = (u16)arg1->spawnArg1.value;
    work->variant         = v;
    if ((s16)v == 1 && arg1->bodyKind == (s16)v) {
        obj->texturePageOffset = obj->texturePageOffset + 1;
        obj->clutRowOffset     = obj->clutRowOffset + 1;
        if (obj->buffer != 0) {
            tmdBuildBufferHalf(obj);
            tmdBuildBufferHalf(obj);
        }
    }
    work->deathPhase   = SUCKLERCEPH_DEATH_PHASE_COUNTDOWN;
    arg1->exitCallback = sucklercephExit;
    arg1->state       += 1;
}
