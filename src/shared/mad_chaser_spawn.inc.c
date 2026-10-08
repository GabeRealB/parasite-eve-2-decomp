/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Binds the ordinary spawn's model, contact, hit-effect and animation storage.
///
/// Requires model == task->extra.tmd, live nine-part coordinates, zeroed work
/// and a live enemy. Model matrices, contacts, poses and slots borrow task-owned
/// work until teardown; parameters, message callbacks and clips borrow the
/// loaded carrier. Sets current/maximum HP and binds the animation context;
/// it leaves slots and pose buffers zeroed until the caller applies a clip.
/// Does not link collision bodies or acquire a battle reference.
static __inline__ void _madChaserBindSpawnResources(Task* task, TmdObject* model, MadChaserWork* work, Enemy* enemy)
{
    enum {
        MAD_CHASER_SPAWN_HIT_EFFECT_LOW_ARG = 320,
        MAD_CHASER_SPAWN_HIT_EFFECT_COUNT   = 2
    };

    task->msgTable             = gMadChaserMsgTable;
    model->lightMtx            = &work->lightMtx;
    model->colorMtx            = &work->colorMtx;
    enemy->param               = &gMadChaserEnemyParams;
    enemy->recs                = work->contacts;
    work->effectArg.coord      = &task->extra.tmd->coords[1];
    work->effectArg.spawnArgLo = MAD_CHASER_SPAWN_HIT_EFFECT_LOW_ARG;
    work->effectArg.spawnArgHi = MAD_CHASER_SPAWN_HIT_EFFECT_COUNT;
    enemy->hp = enemy->hpMax = gMadChaserEnemyParams.hpMax;
    animationInitContext(&work->anim, (AnimationSet**)gMadChaserAnimBank, model, work->poses, work->slots);
}

/// Initializes an ordinary Mad Chaser in lurk or anchored dangle state.
///
/// Requires a live Enemy in spawnArg2.pointer and its nine-part model. Allocates
/// zeroed task-owned work; allocation failure destroys the enemy and starts
/// teardown. Binds its matrices, contacts, animation slots and pose buffers,
/// applies clip 7 at normal rate, links collision and targeting, and acquires
/// one battle reference. The loaded carrier supplies parameters and callbacks.
/// Spawn kind 1 (spawnArg1's low nibble) selects dangle; all others select lurk,
/// both at behavior/sub-state zero. Raises the root by 60 parent-coordinate
/// units and saves XYZ narrowed to s16 as the moving anchor. Storage remains
/// live until task teardown.
static void _madChaserSpawn(Task* task)
{
    enum {
        MAD_CHASER_SPAWN_KIND_MASK    = 0xF,
        MAD_CHASER_SPAWN_INITIAL_CLIP = 7,
        MAD_CHASER_SPAWN_ROOT_LIFT    = 60
    };
    Enemy*         enemy;
    GfxCoord*      root;
    MadChaserWork* work;
    TmdObject*     liveModel;
    MadChaserWork* setupWork;
    Enemy*         setupEnemy;
    GfxCoord*      liveRoot;
    MadChaserWork* animationWork;
    s32            lurkTaskState;

    enemy      = task->spawnArg2.pointer;
    root       = task->extra.tmd->coords;
    task->work = memCalloc(sizeof(MadChaserWork), 0);
    work       = task->work;
    if (work == NULL) {
        enemyDestroy(enemy, task);
        return;
    }
    _madChaserQueueSoundBank();
    // Reload live resources after the sound request, then apply the initial pose.
    liveModel  = task->extra.tmd;
    setupWork  = task->work;
    setupEnemy = task->spawnArg2.pointer;
    liveRoot   = liveModel->coords;
    _madChaserBindSpawnResources(task, liveModel, setupWork, setupEnemy);
    animationWork              = task->work;
    animationWork->animRate    = ANIMATION_RATE_ONE;
    animationWork->animId      = MAD_CHASER_SPAWN_INITIAL_CLIP;
    animationWork->animRequest = MAD_CHASER_ANIM_REQUEST_RESET;
    _madChaserTickAnim(task);
    liveRoot->parent = &gGfxViewCoord;
    _madChaserLinkBodies(task);
    setupWork->rotation.vy = ratan2(-liveRoot->coord.m[2][0], liveRoot->coord.m[2][2]) + ACTOR_TRANSFORM_ANGLE_HALF_TURN;
    enemy                  = task->spawnArg2.pointer;
    worldTargetLinkNode(&enemy->node);
    enemy->field_4                = &task->extra.tmd->coords->coord;
    enemy->field_48               = 0;
    enemy->bodyPos.vx             = 0;
    enemy->bodyPos.vy             = 0;
    enemy->bodyPos.vz             = 0;
    enemy->coord                  = &task->extra.tmd->coords[1];
    enemy->node.state.parts.flags = WORLD_TARGET_KEEP_SCANNED;
    lurkTaskState                 = MAD_CHASER_TASK_LURK;
    (sceneAcquireBattleRef)(0);
    if ((task->spawnArg1.value & MAD_CHASER_SPAWN_KIND_MASK) == lurkTaskState) {
        _madChaserEnterTaskState(task, MAD_CHASER_TASK_DANGLE);
    } else {
        _madChaserEnterTaskState(task, lurkTaskState);
    }
    // Keep the anchor in the root's parent frame for the dangle and contact steps.
    work->anchorPos.vx = root->coord.t[0];
    root->coord.t[1]  -= MAD_CHASER_SPAWN_ROOT_LIFT;
    work->anchorPos.vy = root->coord.t[1];
    work->anchorPos.vz = root->coord.t[2];
}
