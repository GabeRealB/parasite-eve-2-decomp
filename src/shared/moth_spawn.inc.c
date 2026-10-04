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
    work  = memCalloc(0x2F4U, false);
    if (work == NULL) {
        enemyDestroy(arg0, arg1);
        return;
    }
    arg1->work              = work;
    obj->flags              = 0;
    coord->composeStamp     = GRAPHICS_COORD_DIRTY;
    obj->texturePageOffset += 1;
    obj->clutRowOffset     += 1;
    tmdProcessStream(obj);
    tmdProcessStream(obj);
    obj->lightMtx  = &work->field_114;
    obj->colorMtx  = &work->field_F4;
    arg0->field_4  = &coord->coord;
    arg0->field_48 = 0;
    Gp_LinkNode(&arg0->node);
    arg0->coord                  = coord;
    arg0->node.state.parts.flags = 0;
    arg0->bodyPos.vx             = 0;
    arg0->bodyPos.vy             = 0;
    arg0->bodyPos.vz             = 0;
    arg0->param                  = &gMothParams;
    arg0->recs                   = &work->field_154;
    arg0->hp                     = (u16)gMothParams.hpMax;
    work->field_224.spawnArgLo   = 0x100;
    work->field_224.spawnArgHi   = 1;
    work->field_224.coord        = coord;
    animationInitContext(&work->rig.anim, gMothAnimSets, obj, work->rig.poses, work->rig.slots);
    for (i = 1; i < 4; i++) {
        animationResetSlot(&work->rig.anim, i, 1);
    }
    (Gp_IncStateF0Ref)(0);
    work->field_2D6               = 1;
    work->field_2AC               = (s32)coord->coord.t[0];
    work->field_2B0               = (s32)coord->coord.t[1];
    work->field_2B4               = (s32)coord->coord.t[2];
    work->field_2DC               = (u16)((Enemy*)arg1->spawnArg2.pointer)->place->yaw;
    work->obj134.coord            = coord;
    work->obj134.context.contacts = &work->field_154;
    work->obj134.pos.vx           = 0;
    work->obj134.pos.vy           = 0;
    work->obj134.pos.vz           = 0;
    work->obj134.key              = 0x30008;
    work->obj134.radius           = 0xFA;
    work->obj134.flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(2, &work->obj134);
    Gp_InitRec18Table(&work->field_154, 1, 0);
    work->obj16C.coord            = coord;
    work->obj16C.context.contacts = &work->rec18C[0];
    work->obj16C.pos.vx           = 0;
    work->obj16C.pos.vy           = 0;
    work->obj16C.pos.vz           = 0;
    work->obj16C.key              = 0x30008;
    work->obj16C.radius           = 0xFA;
    work->obj16C.flags            = WORLD_COLLISION_BODY_SPHERE;
    work->obj134.flags            = (u16)(work->obj134.flags | WORLD_COLLISION_BODY_PAIR_ENABLED);
    Gp_LinkObj(2, &work->obj16C);
    Gp_InitRec18Table(&work->rec18C[0], 4, 0);
    work->obj1EC.coord            = coord;
    work->obj1EC.context.contacts = &work->rec20C;
    work->obj1EC.pos.vx           = 0;
    work->obj1EC.pos.vy           = 0;
    work->obj1EC.pos.vz           = 0;
    work->obj16C.flags            = (u16)(work->obj16C.flags | WORLD_COLLISION_BODY_GRID_ENABLED);
    work->obj1EC.key              = Gp_PackPair(&gMothAttack, 0);
    work->obj1EC.radius           = 0x190;
    work->obj1EC.flags            = (u32)WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(3, &work->obj1EC);
    Gp_InitRec18Table(&work->rec20C, 1, 0);
    work->obj1EC.flags = (u16)(work->obj1EC.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED));
    arg1->state        = 1;
}
