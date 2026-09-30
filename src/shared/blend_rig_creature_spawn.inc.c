/* Part of the blended rig creature library; see blend_rig_creature.h. */

/// Spawn state: allocates the work block (destroying the enemy if that
/// fails), installs the exit callback, binds the model's light and colour
/// matrices to the block, sets up the enemy record and links its node,
/// initialises both animation contexts, seeds clip 1 and ticks once. It then
/// publishes the message table, parents the root to the view, takes its world
/// position as the actor colour, fills the effect record and advances the
/// task to the per-frame driver.
void rigSpawn(GpEnemy* enemy, Task* task)
{
    SVECTOR          unused; // never referenced; only reserves the frame slot the ROM has
    VECTOR           pos;
    TmdObject*       obj;
    TmdObject*       tmd;
    GfxCoord*        coord;
    Actor323000Work* work;
    Actor323000Work* work2;
    Actor323000Work* mem;

    obj        = task->extra.tmd;
    coord      = obj->coords;
    mem        = (Actor323000Work*)memCalloc(0x934, 0);
    work       = mem;
    task->work = mem;
    if (mem == NULL) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }
    task->exitCallback = rigExit;
    work2              = (Actor323000Work*)task->work;
    tmd                = task->extra.tmd;
    tmd->lightMtx      = &work2->light;
    tmd->colorMtx      = &work2->color;
    enemy->field_4     = &task->extra.tmd->coords->coord;
    enemy->field_48    = 0;
    enemy->bodyPos.vx  = 0;
    enemy->bodyPos.vy  = 0;
    enemy->bodyPos.vz  = 0;
    enemy->coord       = &task->extra.tmd->coords[2];
    Gp_LinkNode(&enemy->node);
    enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
    enemy->param                  = &gRigParams;
    enemy->reactionFlags          = 0;
    enemy->hp                     = 0;
    enemy->recs                   = 0;
    func_800B3F84(&work->anim, gRigAnimSource, obj, work->poses, work->slots);
    func_800B3F84(&work->blendAnim, gRigAnimSource, obj, work->blendPoses, work->blendSlots);
    work->field_828 = 2;
    work->field_82E = 1;
    work->field_82A = 0;
    work->field_844 = 0;
    work->field_840 = 0;
    work->field_834 = 0x10;
    work->field_832 = 0x10;
    rigAnimTick(task);
    task->msgTable      = gRigMessages;
    coord->parent       = &gGfxViewCoord;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    Gp_UpdateCoord(coord);
    pos.vx = coord->workm.t[0];
    pos.vy = coord->workm.t[1];
    pos.vz = coord->workm.t[2];
    Gp_UpdateActorColor(enemy, &pos, 0, 0);
    gRigEffectRec.value.coord      = task->extra.tmd->coords;
    gRigEffectRec.value.spawnArgLo = 0x100;
    gRigEffectRec.value.spawnArgHi = 2;
    work->field_0                  = 0;
    task->state++;
}
