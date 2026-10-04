/* Part of the Desert Chaser library; see desert_chaser.h. */

/// Per-frame driver of the live actor: brings the model root's coordinate up
/// to date, takes its world position as the actor colour, flags a state change
/// in `stateEntered`, and runs the state handler `state` selects from a stack copy
/// of `gDesertChaserStates`. Afterwards it walks the origin of the model's
/// third part coordinate up to `gGfxViewCoord` and stores it as the enemy's
/// local position, parented to the view.
void desertChaserFrameState(Enemy* enemy, Task* task)
{
    DesertChaserWork*         work;
    EnemyTaskFuncTable4       sp;
    DesertChaserFrameScratch* scratch;
    GfxCoord*                 walker;
    SVECTOR*                  pos;

    work = task->work;
    gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    sp                                    = gDesertChaserStates;
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    scratch                               = SCRATCH_STACK_RESERVE_BLOCK(DesertChaserFrameScratch);
    actorRenderComposeCoord(task->extra.tmd->coords);
    scratch->rootPos.vx = task->extra.tmd->coords->workm.t[0];
    scratch->rootPos.vy = task->extra.tmd->coords->workm.t[1];
    scratch->rootPos.vz = task->extra.tmd->coords->workm.t[2];
    Gp_UpdateActorColor(enemy, &scratch->rootPos, 0, 0);
    if (work->prevState != work->state) {
        work->stateEntered = 1;
    } else {
        work->stateEntered = 0;
    }
    work->prevState = work->state;
    sp.funcs[work->state](enemy, task);
    scratch->bodyPos.vx = 0;
    scratch->bodyPos.vy = 0;
    scratch->bodyPos.vz = 0;
    {
        SVECTOR  local;
        VECTOR   result;
        s32      flag;
        SVECTOR* localp = &local;

        walker   = &task->extra.tmd->coords[2];
        pos      = &scratch->bodyPos;
        local.vx = scratch->bodyPos.vx;
        local.vy = pos->vy;
        local.vz = pos->vz;
        while (1) {
            if (walker->parent == NULL)
                break;
            if (walker != &gGfxViewCoord) {
                gte_SetTransMatrix(&walker->coord);
                gte_SetRotMatrix(&walker->coord);
                gte_ldv0(localp);
                gte_rtv0tr();
                gte_stlvnl(&result);
                gte_stflg(&flag);
                local.vx = result.vx;
                local.vy = result.vy;
                local.vz = result.vz;
                walker   = walker->parent;
                continue;
            }
            pos->vx = local.vx;
            pos->vy = local.vy;
            pos->vz = local.vz;
            break;
        }
    }
    enemy->bodyPos.vx = scratch->bodyPos.vx;
    enemy->bodyPos.vy = scratch->bodyPos.vy;
    enemy->bodyPos.vz = scratch->bodyPos.vz;
    enemy->coord      = &gGfxViewCoord;
    SCRATCH_STACK_RELEASE_BLOCK(DesertChaserFrameScratch);
}
