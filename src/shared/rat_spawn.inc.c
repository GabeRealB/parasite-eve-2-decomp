/* Part of the Rat library; see rat.h. */

/// The first body's set-up handler: allocate the work block, rebind the
/// model's light and colour matrices into it, then link the four collision
/// nodes and their tables. `&obj->coords[4]` -- the model's fifth
/// coordinate -- is what both the context and the second node hang off.
/// A failed allocation tears the enemy down and leaves the task here.
void ratSpawn(Enemy* ctx, Task* actor)
{
    RatInitWork* work;
    TmdObject*   obj;
    GfxCoord*    coord;
    s32          i;

    obj   = actor->extra.tmd;
    coord = obj->coords;
    work  = memCalloc(0x39CU, false);
    if (work == NULL) {
        Gp_DestroyEnemy(ctx, actor);
        return;
    }
    actor->work         = work;
    obj->flags          = 0;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    obj->lightMtx       = &work->field_1BC;
    obj->colorMtx       = &work->field_19C;
    ctx->field_4        = &coord->coord;
    ctx->field_48       = 0;
    Gp_LinkNode(&ctx->node);
    ctx->coord                  = &actor->extra.tmd->coords[4];
    ctx->node.state.parts.flags = 0;
    ctx->bodyPos.vx             = 0;
    ctx->bodyPos.vy             = 0;
    ctx->bodyPos.vz             = 0;
    ctx->param                  = &gRatParams;
    ctx->recs                   = work->rec2;
    ctx->hp                     = (u16)gRatParams.hpMax;
    work->field_338             = 0x100;
    work->field_33A             = 1;
    work->field_334             = coord;
    func_800B3F84(&work->anim, gRatAnimSets, obj, &work->field_12C, work->slots);
    for (i = 1; i < 7; i++) {
        Gp_AnimResetSlot(&work->anim, i, 1);
    }
    Gp_IncStateF0Ref(0);
    work->field_37E             = 1;
    work->field_380             = 1;
    work->obj1.coord            = coord;
    work->obj1.context.contacts = &work->rec1;
    work->obj1.pos.vx           = 0;
    work->obj1.pos.vy           = 0;
    work->obj1.pos.vz           = 0x2EE;
    work->obj1.key              = 0;
    work->obj1.radius           = 0x12C;
    work->obj1.flags            = (u32)WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(3, &work->obj1);
    Gp_InitRec18Table(&work->rec1, 1, 0);
    work->obj1.flags            = (u16)(work->obj1.flags | WORLD_COLLISION_BODY_PAIR_ENABLED);
    work->obj2.coord            = &actor->extra.tmd->coords[4];
    work->obj2.context.contacts = work->rec2;
    work->obj2.pos.vx           = 0;
    work->obj2.pos.vy           = 0;
    work->obj2.pos.vz           = 0;
    work->obj2.key              = 0x30007;
    work->obj2.radius           = 0x96;
    work->obj2.flags            = (u32)WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(2, &work->obj2);
    Gp_InitRec18Table(work->rec2, ARRAY_SIZE(work->rec2), 0);
    work->obj3.coord            = coord;
    work->obj3.context.contacts = work->rec3;
    work->obj3.pos.vx           = 0;
    work->obj3.pos.vy           = -0xFA;
    work->obj3.pos.vz           = 0;
    work->obj2.flags            = (u16)(work->obj2.flags | WORLD_COLLISION_BODY_PAIR_ENABLED);
    work->obj3.key              = 0x30007;
    work->obj3.radius           = 0xFA;
    work->obj3.flags            = (u32)WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(2, &work->obj3);
    Gp_InitRec18Table(work->rec3, ARRAY_SIZE(work->rec3), 0);
    work->obj4.coord            = coord;
    work->obj4.context.contacts = &work->rec4;
    work->obj4.pos.vx           = 0;
    work->obj4.pos.vy           = 0;
    work->obj4.pos.vz           = 0x1F4;
    work->obj3.flags            = (u16)(work->obj3.flags | (WORLD_COLLISION_BODY_FLOOR_QUERY | WORLD_COLLISION_BODY_GRID_ENABLED));
    work->obj4.key              = Gp_PackPair(&gRatAttack, 0);
    work->obj4.radius           = 0xC8;
    work->obj4.flags            = (u32)WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(3, &work->obj4);
    Gp_InitRec18Table(&work->rec4, 1, 0);
    work->obj4.flags = (u16)(work->obj4.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED));
    actor->state     = 1;
}
