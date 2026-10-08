/* Part of the Desert Chaser library; see desert_chaser.h. */

/// Initializes the scripted Desert Chaser's work, model and message interface.
///
/// The cutscene build requires a live enemy and model with at least three
/// coordinates and its carrier's animation/parameter tables. A zero-filled work
/// allocation owns lighting matrices and both animation contexts until the exit
/// callback frees it. Allocation failure destroys the enemy. Success links a
/// non-lockable, zero-HP target, starts clip 1 at normal rate, binds the root to
/// the view, relights it there and advances to the frame driver. The carrier's
/// unused effect record is initialized but does not own the root coordinate.
static void _desertChaserSpawn(Enemy* enemy, Task* task)
{
    enum {
        DESERT_CHASER_CUTSCENE_INITIAL_CLIP     = 1,
        DESERT_CHASER_CUTSCENE_EFFECT_MAGNITUDE = 0x100,
        DESERT_CHASER_CUTSCENE_EFFECT_COUNT     = 2,
    };
    SVECTOR           unusedFrameVector; // never referenced; only reserves the frame slot the ROM has
    VECTOR            lightingPosition;
    TmdObject*        model;
    TmdObject*        lightingModel;
    GfxCoord*         rootCoord;
    DesertChaserWork* work;
    DesertChaserWork* lightingWork;
    DesertChaserWork* allocatedWork;

    model         = task->extra.tmd;
    rootCoord     = model->coords;
    allocatedWork = memCalloc(sizeof(*allocatedWork), 0);
    work          = allocatedWork;
    task->work    = allocatedWork;
    if (allocatedWork == NULL) {
        enemyDestroy(enemy, task);
        return;
    }
    // The model borrows lighting storage owned by the task work.
    task->exitCallback      = _desertChaserExit;
    lightingWork            = task->work;
    lightingModel           = task->extra.tmd;
    lightingModel->lightMtx = &lightingWork->lightMtx;
    lightingModel->colorMtx = &lightingWork->colorMtx;
    enemy->field_4          = &task->extra.tmd->coords->coord;
    enemy->field_48         = 0;
    enemy->bodyPos.vx       = 0;
    enemy->bodyPos.vy       = 0;
    enemy->bodyPos.vz       = 0;
    enemy->coord            = &task->extra.tmd->coords[2];
    worldTargetLinkNode(&enemy->node);
    enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
    enemy->param                  = &gRigParams;
    enemy->reactionFlags          = 0;
    enemy->hp                     = 0;
    enemy->recs                   = 0;
    // Both rigs share the carrier's animation source, with separate pose storage.
    animationInitContext(&work->rig.anim, (AnimationSet**)gRigAnimSource, model, work->rig.poses, work->rig.slots);
    animationInitContext(&work->blend.anim, (AnimationSet**)gRigAnimSource, model, work->blend.poses, work->blend.slots);
    work->animRequest   = DESERT_CHASER_ANIM_REQUEST_RESET;
    work->animId        = DESERT_CHASER_CUTSCENE_INITIAL_CLIP;
    work->blendActive   = 0;
    work->lookYaw       = 0;
    work->lookYawTarget = 0;
    work->baseRate      = ANIMATION_RATE_ONE;
    work->animRate      = ANIMATION_RATE_ONE;
    _desertChaserAnimTick(task);
    task->msgTable          = gRigMessages;
    rootCoord->parent       = &gGfxViewCoord;
    rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(rootCoord);
    lightingPosition.vx = rootCoord->workm.t[0];
    lightingPosition.vy = rootCoord->workm.t[1];
    lightingPosition.vz = rootCoord->workm.t[2];
    worldCoordUpdateActorColor(enemy, &lightingPosition, 0, 0);
    gRigEffectRec.effectArg.coord      = task->extra.tmd->coords;
    gRigEffectRec.effectArg.spawnArgLo = DESERT_CHASER_CUTSCENE_EFFECT_MAGNITUDE;
    gRigEffectRec.effectArg.spawnArgHi = DESERT_CHASER_CUTSCENE_EFFECT_COUNT;
    work->state                        = DESERT_CHASER_STATE_HIDDEN;
    task->state++;
}
