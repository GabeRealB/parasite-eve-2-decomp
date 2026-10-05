/* Part of the Moth library; see moth.h. */

/// Task state 0: allocates the 0x2F4-byte work block (tearing the enemy down on
/// failure), shifts the model's texture page and CLUT row by one and
/// reprocesses its stream, points the colour and light matrices into the work,
/// and links the enemy node with gMothParams and its hpMax. Binds slots 1-3 to
/// gMothAnimSets, records the home position and the place's yaw, then links
/// three spheres - the hit sphere that is the enemy's contact table, a grid-
/// enabled terrain sphere (both list 2) and a disabled attack sphere keyed with
/// gMothAttack (list 3) - and moves to state 1.
void mothSpawn(Enemy* arg0, Task* arg1)
{
    MothWork*  work;
    GfxCoord*  coord;
    TmdObject* obj;
    s32        i;

    obj   = arg1->extra.tmd;
    coord = obj->coords;
    work  = memCalloc(sizeof(MothWork), false);
    if (work == NULL) {
        enemyDestroy(arg0, arg1);
        return;
    }
    arg1->work              = work;
    obj->flags              = 0;
    coord->composeStamp     = GRAPHICS_COORD_DIRTY;
    obj->texturePageOffset += 1;
    obj->clutRowOffset     += 1;
    tmdBuildBufferHalf(obj);
    tmdBuildBufferHalf(obj);
    obj->lightMtx  = &work->lightMtx;
    obj->colorMtx  = &work->colorMtx;
    arg0->field_4  = &coord->coord;
    arg0->field_48 = 0;
    Gp_LinkNode(&arg0->node);
    arg0->coord                   = coord;
    arg0->node.state.parts.flags  = 0;
    arg0->bodyPos.vx              = 0;
    arg0->bodyPos.vy              = 0;
    arg0->bodyPos.vz              = 0;
    arg0->param                   = &gMothParams;
    arg0->recs                    = work->hitContacts;
    arg0->hp                      = (u16)gMothParams.hpMax;
    work->hitEffectArg.spawnArgLo = 0x100;
    work->hitEffectArg.spawnArgHi = 1;
    work->hitEffectArg.coord      = coord;
    animationInitContext(&work->rig.anim, gMothAnimSets, obj, work->rig.poses, work->rig.slots);
    for (i = 1; i < 4; i++) {
        animationResetSlot(&work->rig.anim, i, 1);
    }
    (Gp_IncStateF0Ref)(0);
    work->flapSign                 = 1;
    work->homePos.vx               = coord->coord.t[0];
    work->homePos.vy               = coord->coord.t[1];
    work->homePos.vz               = coord->coord.t[2];
    work->yaw                      = ((Enemy*)arg1->spawnArg2.pointer)->place->yaw;
    work->hitBody.coord            = coord;
    work->hitBody.context.contacts = work->hitContacts;
    work->hitBody.pos.vx           = 0;
    work->hitBody.pos.vy           = 0;
    work->hitBody.pos.vz           = 0;
    work->hitBody.key              = 0x30008;
    work->hitBody.radius           = 0xFA;
    work->hitBody.flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(2, &work->hitBody);
    worldCollisionInitContacts(work->hitContacts, ARRAY_SIZE(work->hitContacts), 0);
    work->gridBody.coord            = coord;
    work->gridBody.context.contacts = work->gridContacts;
    work->gridBody.pos.vx           = 0;
    work->gridBody.pos.vy           = 0;
    work->gridBody.pos.vz           = 0;
    work->gridBody.key              = 0x30008;
    work->gridBody.radius           = 0xFA;
    work->gridBody.flags            = WORLD_COLLISION_BODY_SPHERE;
    work->hitBody.flags             = (u16)(work->hitBody.flags | WORLD_COLLISION_BODY_PAIR_ENABLED);
    Gp_LinkObj(2, &work->gridBody);
    worldCollisionInitContacts(work->gridContacts, ARRAY_SIZE(work->gridContacts), 0);
    work->attackBody.coord            = coord;
    work->attackBody.context.contacts = work->attackContacts;
    work->attackBody.pos.vx           = 0;
    work->attackBody.pos.vy           = 0;
    work->attackBody.pos.vz           = 0;
    work->gridBody.flags              = (u16)(work->gridBody.flags | WORLD_COLLISION_BODY_GRID_ENABLED);
    work->attackBody.key              = Gp_PackPair(&gMothAttack, 0);
    work->attackBody.radius           = 0x190;
    work->attackBody.flags            = (u32)WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(3, &work->attackBody);
    worldCollisionInitContacts(work->attackContacts, ARRAY_SIZE(work->attackContacts), 0);
    work->attackBody.flags = (u16)(work->attackBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED));
    arg1->state            = 1;
}
