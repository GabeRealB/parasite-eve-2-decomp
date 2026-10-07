/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Binds hidden-spawn resources to the task-owned work and loaded model.
///
/// Model matrices, enemy contacts and animation storage borrow work until
/// task teardown; the clip bank and message table remain overlay-owned.
/// Requires model == task->extra.tmd, live nine-part coordinates, zeroed work,
/// a live enemy and a loaded clip bank. Neither storage nor a battle reference
/// is allocated here; the caller owns initialization of slots and collision.
static __inline__ void _madChaserBindHiddenResources(Task* task, TmdObject* model, MadChaserWork* work, Enemy* enemy)
{
    enum {
        MAD_CHASER_HIDDEN_HIT_EFFECT_SPAN  = 320,
        MAD_CHASER_HIDDEN_HIT_EFFECT_COUNT = 2,
    };
    task->msgTable             = gMadChaserMsgTable;
    model->lightMtx            = &work->lightMtx;
    model->colorMtx            = &work->colorMtx;
    enemy->param               = &gMadChaserEnemyParams;
    enemy->recs                = work->contacts;
    work->effectArg.coord      = &task->extra.tmd->coords[1];
    work->effectArg.spawnArgLo = MAD_CHASER_HIDDEN_HIT_EFFECT_SPAN;
    work->effectArg.spawnArgHi = MAD_CHASER_HIDDEN_HIT_EFFECT_COUNT;
    enemy->hp = enemy->hpMax = gMadChaserEnemyParams.hpMax;
    animationInitContext(&work->anim, (AnimationSet**)gMadChaserAnimBank, model, work->poses, work->slots);
}

/// Initializes a Mad Chaser that waits for a scripted emerge command.
///
/// Requires a live enemy in spawnArg2.pointer and its nine-part model. Owns a
/// zeroed MadChaserWork allocation, whose matrices, contacts, slots and pose
/// buffers remain borrowed by the model, enemy and animation context until task
/// teardown. Acquires one battle reference and links an un-lockable target.
/// Disables pair/grid collision and limb shadows, raises the root by 60 parent
/// units and enters emerge behavior zero. Spawn kind 2 also hides active drawing.
/// Bit 16 of spawnArg1.value cancels the spawn after the sound-bank request;
/// allocation failure and cancellation destroy the enemy and begin task teardown.
static void _madChaserSpawnHidden(Task* task)
{
    enum {
        MAD_CHASER_HIDDEN_SPAWN_CANCEL_SHIFT    = 16,
        MAD_CHASER_HIDDEN_SPAWN_KIND_MASK       = 0xF,
        MAD_CHASER_HIDDEN_SPAWN_HIDE_MODEL_KIND = 2,
        MAD_CHASER_HIDDEN_INITIAL_CLIP          = 7,
        MAD_CHASER_HIDDEN_ROOT_LIFT             = 60,
    };
    TmdObject*     model;
    Enemy*         enemy;
    GfxCoord*      root;
    MadChaserWork* work;
    TmdObject*     liveModel;
    MadChaserWork* setupWork;
    Enemy*         setupEnemy;
    GfxCoord*      liveRoot;
    MadChaserWork* animationWork;
    MadChaserWork* stateWork;
    Enemy*         targetEnemy;
    s32            spawnFlags;

    model      = task->extra.tmd;
    enemy      = task->spawnArg2.pointer;
    root       = model->coords;
    task->work = memCalloc(sizeof(MadChaserWork), 0);
    work       = task->work;
    if (work == NULL) {
        enemyDestroy(enemy, task);
        return;
    }
    _madChaserQueueSoundBank();
    spawnFlags = task->spawnArg1.value;
    if ((spawnFlags >> MAD_CHASER_HIDDEN_SPAWN_CANCEL_SHIFT) & 1) {
        enemyDestroy(enemy, task);
        return;
    }
    if ((spawnFlags & MAD_CHASER_HIDDEN_SPAWN_KIND_MASK) == MAD_CHASER_HIDDEN_SPAWN_HIDE_MODEL_KIND) {
        model->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
    }
    // Bind task-owned storage before applying the initial pose.
    liveModel  = task->extra.tmd;
    setupWork  = task->work;
    setupEnemy = task->spawnArg2.pointer;
    liveRoot   = liveModel->coords;
    _madChaserBindHiddenResources(task, liveModel, setupWork, setupEnemy);
    animationWork              = task->work;
    animationWork->animRate    = ANIMATION_RATE_ONE;
    animationWork->animId      = MAD_CHASER_HIDDEN_INITIAL_CLIP;
    animationWork->animRequest = MAD_CHASER_ANIM_REQUEST_RESET;
    _madChaserTickAnim(task);
    liveRoot->parent = &gGfxViewCoord;
    _madChaserLinkBodies(task);
    setupWork->rotation.vy = ratan2(-liveRoot->coord.m[2][0], liveRoot->coord.m[2][2]) + ACTOR_TRANSFORM_ANGLE_HALF_TURN;
    (sceneAcquireBattleRef)(0);
    targetEnemy = task->spawnArg2.pointer;
    worldTargetLinkNode(&targetEnemy->node);
    targetEnemy->field_4                = &task->extra.tmd->coords->coord;
    targetEnemy->field_48               = 0;
    targetEnemy->bodyPos.vx             = 0;
    targetEnemy->bodyPos.vy             = 0;
    targetEnemy->bodyPos.vz             = 0;
    targetEnemy->coord                  = &task->extra.tmd->coords[1];
    targetEnemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
    // Retain the spawn anchor while disabling contact and shadow presentation.
    work->anchorPos.vx    = root->coord.t[0];
    root->coord.t[1]     -= MAD_CHASER_HIDDEN_ROOT_LIFT;
    work->anchorPos.vy    = root->coord.t[1];
    work->anchorPos.vz    = root->coord.t[2];
    work->shadowHidden    = 1;
    work->pairBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    work->gridBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
    stateWork             = task->work;
    task->state           = MAD_CHASER_TASK_EMERGE;
    stateWork->state      = 0;
    stateWork->subState   = 0;
}
