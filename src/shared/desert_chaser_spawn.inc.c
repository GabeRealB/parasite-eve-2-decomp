/* Part of the Desert Chaser library; see desert_chaser.h. */

/// Spawn state: allocates the work block (destroying the enemy if that
/// fails), installs the exit callback, binds the model's light and colour
/// matrices to the block, sets up the enemy record and links its node,
/// initialises both animation contexts, seeds clip 1 and ticks once. It then
/// publishes the message table, parents the root to the view, takes its world
/// position as the actor colour, fills the effect record and advances the
/// task to the per-frame driver.
void desertChaserSpawn(Enemy* enemy, Task* task)
{
    SVECTOR           unused; // never referenced; only reserves the frame slot the ROM has
    VECTOR            pos;
    TmdObject*        obj;
    TmdObject*        tmd;
    GfxCoord*         coord;
    DesertChaserWork* work;
    DesertChaserWork* work2;
    DesertChaserWork* mem;

    obj        = task->extra.tmd;
    coord      = obj->coords;
    mem        = memCalloc(sizeof(DesertChaserWork), 0);
    work       = mem;
    task->work = mem;
    if (mem == NULL) {
        enemyDestroy(enemy, task);
        return;
    }
    task->exitCallback = desertChaserExit;
    work2              = task->work;
    tmd                = task->extra.tmd;
    tmd->lightMtx      = &work2->lightMtx;
    tmd->colorMtx      = &work2->colorMtx;
    enemy->field_4     = &task->extra.tmd->coords->coord;
    enemy->field_48    = 0;
    enemy->bodyPos.vx  = 0;
    enemy->bodyPos.vy  = 0;
    enemy->bodyPos.vz  = 0;
    enemy->coord       = &task->extra.tmd->coords[2];
    worldTargetLinkNode(&enemy->node);
    enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
    enemy->param                  = &gRigParams;
    enemy->reactionFlags          = 0;
    enemy->hp                     = 0;
    enemy->recs                   = 0;
    animationInitContext(&work->rig.anim, (AnimationSet**)gRigAnimSource, obj, work->rig.poses, work->rig.slots);
    animationInitContext(&work->blend.anim, (AnimationSet**)gRigAnimSource, obj, work->blend.poses, work->blend.slots);
    work->animRequest   = DESERT_CHASER_ANIM_REQUEST_RESET;
    work->animId        = 1;
    work->blendActive   = 0;
    work->lookYaw       = 0;
    work->lookYawTarget = 0;
    work->baseRate      = 0x10;
    work->animRate      = 0x10;
    desertChaserAnimTick(task);
    task->msgTable      = gRigMessages;
    coord->parent       = &gGfxViewCoord;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(coord);
    pos.vx = coord->workm.t[0];
    pos.vy = coord->workm.t[1];
    pos.vz = coord->workm.t[2];
    Gp_UpdateActorColor(enemy, &pos, 0, 0);
    gRigEffectRec.effectArg.coord      = task->extra.tmd->coords;
    gRigEffectRec.effectArg.spawnArgLo = 0x100;
    gRigEffectRec.effectArg.spawnArgHi = 2;
    work->state                        = 0;
    task->state++;
}
