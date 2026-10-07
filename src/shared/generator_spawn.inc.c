/* Part of the Generator library; see generator.h. */

void generatorSpawn(Enemy* arg0, Task* arg1)
{
    GeneratorWork*   work;
    TmdObject*       obj;
    GfxCoord*        coord;
    GameLocationKey* sessionKey;
    AreaVariant*     layout;
    AreaPlacement*   place;
    TmdObject*       model;
    Enemy*           spawned;
    GameLocationKey  key;
    u16              idx;
    s32              sound;
    s32              i;

    obj   = arg1->extra.tmd;
    coord = obj->coords;
    work  = memCalloc(sizeof(GeneratorWork), 0);
    if (work == NULL) {
        enemyDestroy(arg0, arg1);
        return;
    }
    arg1->work          = work;
    obj->flags          = 0;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    obj->lightMtx       = &work->lightMtx;
    obj->colorMtx       = &work->colorMtx;
    arg0->field_4       = &coord->coord;
    arg0->field_48      = 0;
    worldTargetLinkNode(&arg0->node);
    arg0->coord                = coord;
    arg0->bodyPos.vx           = gGeneratorSpawnOffsets[GENERATOR_KIND].vx;
    arg0->bodyPos.vy           = gGeneratorSpawnOffsets[GENERATOR_KIND].vy;
    arg0->bodyPos.vz           = gGeneratorSpawnOffsets[GENERATOR_KIND].vz;
    arg0->param                = &gGeneratorParams;
    arg0->recs                 = work->contacts;
    arg0->hp                   = gGeneratorParams.hpMax;
    work->effectArg.coord      = coord;
    work->effectArg.spawnArgLo = 0x500;
    work->effectArg.spawnArgHi = 3;
    animationInitContext(&work->anim, gGeneratorAnimSets, obj, work->poses, work->slots);
    for (i = 1; i < ARRAY_SIZE(work->slots); i++) {
        animationResetSlot(&work->anim, i, GENERATOR_ANIM_IDLE);
    }
    (sceneAcquireBattleRef)(0);
    work->kind                      = GENERATOR_KIND;
    work->shrinkScale               = ONE;
    work->unscaledMtx               = coord->coord;
    work->alive                     = 1;
    work->hpCeiling                 = gGeneratorParams.hpMax;
    work->rootBody.coord            = coord;
    work->rootBody.context.contacts = work->contacts;
    work->rootBody.pos.vx           = 0;
    work->rootBody.pos.vy           = 0;
    work->rootBody.pos.vz           = 0;
    work->rootBody.key              = GENERATOR_COLLISION_KEY;
    work->rootBody.radius           = 0x5DC;
    work->rootBody.flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, &work->rootBody);
    worldCollisionInitContacts(work->contacts, ARRAY_SIZE(work->contacts), 0);
    work->rootBody.flags             |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    work->targetBody.coord            = coord;
    work->targetBody.context.contacts = work->contacts;
    work->targetBody.pos.vx           = gGeneratorSpawnOffsets[GENERATOR_KIND].vx;
    work->targetBody.pos.vy           = gGeneratorSpawnOffsets[GENERATOR_KIND].vy;
    work->targetBody.pos.vz           = gGeneratorSpawnOffsets[GENERATOR_KIND].vz;
    work->targetBody.key              = GENERATOR_COLLISION_KEY;
    work->targetBody.radius           = 0x12C;
    work->targetBody.flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, &work->targetBody);
    work->targetBody.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    spawned                 = enemySpawnFromTable(gGeneratorTasks, 1, 0, arg0);
    model                   = spawned->task->extra.tmd;
    idx                     = arg0->placeKey >> ENEMY_PLACE_INDEX_SHIFT;
    sessionKey              = &gGameSession->location.loc;
    key.stage               = sessionKey->stage;
    key.area                = sessionKey->area;
    key.room                = sessionKey->room;
    key.view                = sessionKey->view;
    areaSyncLocationVariant(&key);
    layout                   = Gp_GetNestedAreaRec(&key);
    place                    = gpAreaPlaceAt(layout->placements, idx);
    model->texturePageOffset = place->texturePageOffset;
    model->clutRowOffset     = place->clutRowOffset;
    if (model->buffer != NULL) {
        tmdBuildBufferHalf(model);
        tmdBuildBufferHalf(model);
    }
    sound                = gGeneratorSpawnSound | ((((Enemy*)arg1->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
    work->runningSoundId = sound;
    sndEvtRequestScriptStart(sound, (s8)gGeneratorViewSound[gGameSession->location.loc.view].panOffset,
                             (s8)gGeneratorViewSound[gGameSession->location.loc.view].attenuation);
    arg1->msgTable = gGeneratorMessages;
    arg1->state    = 1;
}
