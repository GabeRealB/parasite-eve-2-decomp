/* Part of the Generator library; see generator.h. */

void generatorSpawn(Enemy* arg0, Task* arg1)
{
    GeneratorWork*   work;
    TmdObject*       obj;
    GfxCoord*        coord;
    GameLocationKey* sessionKey;
    GpAreaVariant*   rec;
    AreaPlacement*   place;
    TmdObject*       model;
    Enemy*           spawned;
    GameLocationKey  key;
    u16              idx;
    s32              sound;
    s32              i;

    obj   = arg1->extra.tmd;
    coord = obj->coords;
    work  = memCalloc(0x340, 0);
    if (work == NULL) {
        enemyDestroy(arg0, arg1);
        return;
    }
    arg1->work          = work;
    obj->flags          = 0;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    obj->lightMtx       = &work->field_264;
    obj->colorMtx       = &work->field_244;
    arg0->field_4       = &coord->coord;
    arg0->field_48      = 0;
    Gp_LinkNode(&arg0->node);
    arg0->coord                = coord;
    arg0->bodyPos.vx           = gGeneratorSpawnOffsets[GENERATOR_KIND].vx;
    arg0->bodyPos.vy           = gGeneratorSpawnOffsets[GENERATOR_KIND].vy;
    arg0->bodyPos.vz           = gGeneratorSpawnOffsets[GENERATOR_KIND].vz;
    arg0->param                = &gGeneratorParams;
    arg0->recs                 = work->rec18;
    arg0->hp                   = gGeneratorParams.hpMax;
    work->field_2F4.coord      = coord;
    work->field_2F4.spawnArgLo = 0x500;
    work->field_2F4.spawnArgHi = 3;
    animationInitContext(&work->anim, gGeneratorAnimSets, obj,
                         (u8(*)[ANIMATION_POSE_BUFFER_BYTES])work->poses, work->slots);
    for (i = 1; i < 0xA; i++) {
        animationResetSlot(&work->anim, i, 1);
    }
    (Gp_IncStateF0Ref)(0);
    work->kind                   = GENERATOR_KIND;
    work->field_326              = 0x1000;
    work->field_2FC              = coord->coord;
    work->field_338              = 1;
    work->field_33C              = gGeneratorParams.hpMax;
    work->node0.coord            = coord;
    work->node0.context.contacts = work->rec18;
    work->node0.pos.vx           = 0;
    work->node0.pos.vy           = 0;
    work->node0.pos.vz           = 0;
    work->node0.key              = GENERATOR_COLLISION_KEY;
    work->node0.radius           = 0x5DC;
    work->node0.flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(2, &work->node0);
    Gp_InitRec18Table(work->rec18, 2, 0);
    work->node0.flags           |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    work->node1.coord            = coord;
    work->node1.context.contacts = work->rec18;
    work->node1.pos.vx           = gGeneratorSpawnOffsets[GENERATOR_KIND].vx;
    work->node1.pos.vy           = gGeneratorSpawnOffsets[GENERATOR_KIND].vy;
    work->node1.pos.vz           = gGeneratorSpawnOffsets[GENERATOR_KIND].vz;
    work->node1.key              = GENERATOR_COLLISION_KEY;
    work->node1.radius           = 0x12C;
    work->node1.flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(2, &work->node1);
    work->node1.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    spawned            = Gp_SpawnEnemyFromTable(gGeneratorTasks, 1, 0, arg0);
    model              = spawned->task->extra.tmd;
    idx                = arg0->placeKey >> ENEMY_PLACE_INDEX_SHIFT;
    sessionKey         = &gGameSession->location.loc;
    key.stage          = sessionKey->stage;
    key.area           = sessionKey->area;
    key.room           = sessionKey->room;
    key.view           = sessionKey->view;
    areaSyncLocationVariant(&key);
    rec                      = Gp_GetNestedAreaRec(&key);
    place                    = gpAreaPlaceAt(rec->field_0, idx);
    model->texturePageOffset = place->texturePageOffset;
    model->clutRowOffset     = place->clutRowOffset;
    if (model->buffer != NULL) {
        tmdProcessStream(model);
        tmdProcessStream(model);
    }
    sound           = gGeneratorSpawnSound | ((((Enemy*)arg1->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
    work->field_31C = sound;
    SndEvt_EnqueueType6(sound, gGeneratorViewSound[gGameSession->location.loc.view].field_0,
                        gGeneratorViewSound[gGameSession->location.loc.view].field_2);
    arg1->msgTable = gGeneratorMessages;
    arg1->state    = 1;
}
