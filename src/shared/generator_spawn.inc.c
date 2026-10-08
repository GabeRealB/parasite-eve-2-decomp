/* Part of the Generator library; see generator.h. */

/// Initializes a generator body, its animation rig and collision, and its child part.
///
/// Requires a model task whose spawnArg2.pointer is enemy, and a placement
/// index valid in the synchronized area variant. Allocates zeroed primary-heap
/// GeneratorWork owned by the task; allocation failure destroys the enemy.
/// Initializes slots 1..9, borrows the package's animation and message tables,
/// acquires one battle reference and parents a Life Support task to the body.
/// Child spawning is assumed to succeed, as in the original implementation.
/// Installs the running sound and enters the active state.
static void _generatorSpawn(Enemy* enemy, Task* task)
{
    enum {
        GENERATOR_BODY_RADIUS             = 1500,
        GENERATOR_TARGET_RADIUS           = 300,
        GENERATOR_LIFE_SUPPORT_TASK_INDEX = 1,
        GENERATOR_HIT_EFFECT_MAGNITUDE    = 1280,
        GENERATOR_BODY_HIT_EFFECT_COUNT   = 3
    };
    GeneratorWork*        work;
    TmdObject*            bodyModel;
    GfxCoord*             rootCoord;
    GameLocationKey*      sessionLocation;
    AreaVariant*          areaVariant;
    AreaPlacement*        placement;
    ModelObjectCoordBody* lifeSupportBody;
    TmdObject*            lifeSupportModelView;
    Enemy*                lifeSupportEnemy;
    GameLocationKey       location;
    u16                   placementIndex;
    s32                   soundId;
    s32                   slotIndex;

    bodyModel = task->extra.tmd;
    rootCoord = bodyModel->coords;
    work      = memCalloc(sizeof(GeneratorWork), 0);
    if (work == NULL) {
        enemyDestroy(enemy, task);
        return;
    }
    task->work              = work;
    bodyModel->flags        = 0;
    rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    bodyModel->lightMtx     = &work->lightMtx;
    bodyModel->colorMtx     = &work->colorMtx;
    enemy->field_4          = &rootCoord->coord;
    enemy->field_48         = 0;
    worldTargetLinkNode(&enemy->node);
    enemy->coord               = rootCoord;
    enemy->bodyPos.vx          = gGeneratorSpawnOffsets[GENERATOR_KIND].vx;
    enemy->bodyPos.vy          = gGeneratorSpawnOffsets[GENERATOR_KIND].vy;
    enemy->bodyPos.vz          = gGeneratorSpawnOffsets[GENERATOR_KIND].vz;
    enemy->param               = &gGeneratorParams;
    enemy->recs                = work->contacts;
    enemy->hp                  = gGeneratorParams.hpMax;
    work->effectArg.coord      = rootCoord;
    work->effectArg.spawnArgLo = GENERATOR_HIT_EFFECT_MAGNITUDE;
    work->effectArg.spawnArgHi = GENERATOR_BODY_HIT_EFFECT_COUNT;
    // The task owns the context and ten encoded pose buffers for the rig.
    animationInitContext(&work->anim, gGeneratorAnimSets, bodyModel, work->poses, work->slots);
    for (slotIndex = 1; slotIndex < ARRAY_SIZE(work->slots); slotIndex++) {
        animationResetSlot(&work->anim, slotIndex, GENERATOR_ANIM_IDLE);
    }
    sceneAcquireBattleRef(0);
    work->kind        = GENERATOR_KIND;
    work->shrinkScale = ONE;
    work->unscaledMtx = rootCoord->coord;
    work->alive       = 1;
    work->hpCeiling   = gGeneratorParams.hpMax;
    // Both spheres share the two-contact table; the target sphere uses bodyPos.
    work->rootBody.coord            = rootCoord;
    work->rootBody.context.contacts = work->contacts;
    work->rootBody.pos.vx           = 0;
    work->rootBody.pos.vy           = 0;
    work->rootBody.pos.vz           = 0;
    work->rootBody.key              = GENERATOR_COLLISION_KEY;
    work->rootBody.radius           = GENERATOR_BODY_RADIUS;
    work->rootBody.flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, &work->rootBody);
    worldCollisionInitContacts(work->contacts, ARRAY_SIZE(work->contacts), 0);
    work->rootBody.flags             |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    work->targetBody.coord            = rootCoord;
    work->targetBody.context.contacts = work->contacts;
    work->targetBody.pos.vx           = gGeneratorSpawnOffsets[GENERATOR_KIND].vx;
    work->targetBody.pos.vy           = gGeneratorSpawnOffsets[GENERATOR_KIND].vy;
    work->targetBody.pos.vz           = gGeneratorSpawnOffsets[GENERATOR_KIND].vz;
    work->targetBody.key              = GENERATOR_COLLISION_KEY;
    work->targetBody.radius           = GENERATOR_TARGET_RADIUS;
    work->targetBody.flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, &work->targetBody);
    work->targetBody.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    lifeSupportEnemy        = enemySpawnFromTable(gGeneratorTasks, GENERATOR_LIFE_SUPPORT_TASK_INDEX, 0, enemy);
    // Keep the original TMD texture/buffer operations on the coordinate-only child.
    // These offsets address bytes in ownedCoord; their purpose is unproven.
    lifeSupportBody      = lifeSupportEnemy->task->extra.coordBody;
    lifeSupportModelView = (TmdObject*)lifeSupportBody;
    placementIndex       = enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT;
    sessionLocation      = &gGameSession->location.loc;
    location.stage       = sessionLocation->stage;
    location.area        = sessionLocation->area;
    location.room        = sessionLocation->room;
    location.view        = sessionLocation->view;
    areaSyncLocationVariant(&location);
    areaVariant                             = areaGetVariant(&location);
    placement                               = gpAreaPlaceAt(areaVariant->placements, placementIndex);
    lifeSupportModelView->texturePageOffset = placement->texturePageOffset;
    lifeSupportModelView->clutRowOffset     = placement->clutRowOffset;
    if (lifeSupportModelView->buffer != NULL) {
        tmdBuildBufferHalf(lifeSupportModelView);
        tmdBuildBufferHalf(lifeSupportModelView);
    }
    soundId              = gGeneratorSpawnSound | ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
    work->runningSoundId = soundId;
    sndEvtRequestScriptStart(soundId, (s8)gGeneratorViewSound[gGameSession->location.loc.view].panOffset,
                             (s8)gGeneratorViewSound[gGameSession->location.loc.view].attenuation);
    task->msgTable = gGeneratorMessages;
    task->state    = GENERATOR_TASK_ACTIVE;
}
