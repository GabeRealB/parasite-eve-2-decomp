/* Part of the Generator library; see generator.h. */

/// Spawn state of the enemy: allocates the 0x48-byte part object, seeds its
/// coordinate's translation from the sub-state's entry in
/// `gGeneratorLifeSupportPos`, links it into `Gp_ObjLists[2]`, and raises one of
/// the two per-enemy death flags. A failed allocation tears the enemy down
/// instead and leaves the task on this handler; otherwise the task moves to the
/// tick handler (`state` 1).
void generatorLifeSupportSpawn(Enemy* arg0, Task* arg1)
{
    TmdObject*             obj;
    GeneratorWork*         work;
    GeneratorPart*         part;
    GfxCoord*              coord;
    WorldCollisionContact* rec18;
    s32                    flag;
    u16                    type;

    obj   = arg1->extra.tmd;
    coord = obj->coords;
    work  = arg1->parent->work;
    part  = memCalloc(0x48, 0);
    if (part == NULL) {
        enemyDestroy(arg0, arg1);
        return;
    }
    arg1->work          = part;
    coord->parent       = &gGfxViewCoord;
    coord->coord.t[0]   = gGeneratorLifeSupportPos[work->kind].x;
    coord->coord.t[1]   = gGeneratorLifeSupportPos[work->kind].y;
    coord->coord.t[2]   = gGeneratorLifeSupportPos[work->kind].z;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    arg0->field_4       = &coord->coord;
    arg0->field_48      = 0;
    Gp_LinkNode(&arg0->node);
    rec18                      = part->rec18;
    arg0->coord                = coord;
    arg0->bodyPos.vx           = 0;
    arg0->bodyPos.vy           = 0;
    arg0->bodyPos.vz           = 0;
    arg0->param                = &gGeneratorLifeSupportParams;
    arg0->recs                 = rec18;
    arg0->hp                   = gGeneratorLifeSupportParams.hpMax;
    part->field_38.spawnArgLo  = 0x500;
    part->field_38.coord       = coord;
    part->field_38.spawnArgHi  = 2;
    part->obj.coord            = coord;
    part->obj.context.contacts = rec18;
    part->obj.pos.vx           = 0;
    part->obj.pos.vy           = 0;
    part->obj.pos.vz           = 0;
    part->obj.key              = ((GeneratorWork*)arg1->parent->work)->rootBody.key;
    part->obj.radius           = 0xC8;
    part->obj.flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(2, &part->obj);
    Gp_InitRec18Table(rec18, 1, 0);
    part->obj.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    type             = work->kind;
    part->field_46   = type;
    if ((type << 0x10) == 0) {
        func_neo_ark_power_plant_2_8017FD88(1);
        flag = 0x147;
    } else {
        func_neo_ark_power_plant_1_8017E524(1);
        flag = 0x148;
    }
    GameFlag_SetNibble(flag, 0);
    arg1->state = 1;
}
