/* Part of the Generator library; see generator.h. */

/// Spawn state of the enemy: allocates its `GeneratorLifeSupportWork`, seeds its
/// coordinate's translation from the kind's entry in
/// `gGeneratorLifeSupportPos`, links it into `Gp_ObjLists[2]`, and raises one of
/// the two per-enemy death flags. A failed allocation tears the enemy down
/// instead and leaves the task on this handler; otherwise the task moves to the
/// tick handler (`state` 1).
void generatorLifeSupportSpawn(Enemy* arg0, Task* arg1)
{
    TmdObject*                obj;
    GeneratorWork*            work;
    GeneratorLifeSupportWork* part;
    GfxCoord*                 coord;
    WorldCollisionContact*    contacts;
    s32                       flag;
    u16                       type;

    obj   = arg1->extra.tmd;
    coord = obj->coords;
    work  = arg1->parent->work;
    part  = memCalloc(sizeof(GeneratorLifeSupportWork), 0);
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
    worldTargetLinkNode(&arg0->node);
    contacts                    = part->contacts;
    arg0->coord                 = coord;
    arg0->bodyPos.vx            = 0;
    arg0->bodyPos.vy            = 0;
    arg0->bodyPos.vz            = 0;
    arg0->param                 = &gGeneratorLifeSupportParams;
    arg0->recs                  = contacts;
    arg0->hp                    = gGeneratorLifeSupportParams.hpMax;
    part->effectArg.spawnArgLo  = 0x500;
    part->effectArg.coord       = coord;
    part->effectArg.spawnArgHi  = 2;
    part->body.coord            = coord;
    part->body.context.contacts = contacts;
    part->body.pos.vx           = 0;
    part->body.pos.vy           = 0;
    part->body.pos.vz           = 0;
    part->body.key              = ((GeneratorWork*)arg1->parent->work)->rootBody.key;
    part->body.radius           = 0xC8;
    part->body.flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, &part->body);
    worldCollisionInitContacts(contacts, ARRAY_SIZE(part->contacts), 0);
    part->body.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    type              = work->kind;
    part->kind        = type;
    if ((type << 0x10) == 0) {
        neoArkPowerPlant2SetView6SpritesHidden(1);
        flag = 0x147;
    } else {
        func_neo_ark_power_plant_1_8017E524(1);
        flag = 0x148;
    }
    gameFlagSetNibble(flag, 0);
    arg1->state = 1;
}
