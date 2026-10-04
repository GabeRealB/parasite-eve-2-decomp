/* Part of the Desert Chaser library; see desert_chaser.h. */

/// Per-frame driver of the live actor: brings the model root's coordinate up
/// to date, takes its world position as the actor colour, flags a state change
/// in `stateEntered`, and runs the state handler `state` selects from a stack copy
/// of `gDesertChaserStates`. Afterwards it walks the origin of the model's
/// third part coordinate up to `gGfxViewCoord` and stores it as the enemy's
/// local position, parented to the view.
void desertChaserFrameState(Enemy* enemy, Task* task)
{
    DesertChaserWork*        work;
    EnemyTaskFuncTable4      sp;
    DesertChaserTickScratch* scratch;
    u8*                      head;
    GfxCoord*                walker;
    SVECTOR*                 pos;

    work = task->work;
    gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    sp                                    = gDesertChaserStates;
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    head                                  = SCRATCH_STACK_CURSOR(u8);
    SCRATCH_STACK_CURSOR(u8)              = head - 0x1C;
    scratch                               = (DesertChaserTickScratch*)(head - 0x1C);
    Gp_UpdateCoord(task->extra.tmd->coords);
    scratch->pos.vx = task->extra.tmd->coords->workm.t[0];
    scratch->pos.vy = task->extra.tmd->coords->workm.t[1];
    scratch->pos.vz = task->extra.tmd->coords->workm.t[2];
    Gp_UpdateActorColor(enemy, &scratch->pos, 0, 0);
    if (work->prevState != work->state) {
        work->stateEntered = 1;
    } else {
        work->stateEntered = 0;
    }
    work->prevState = work->state;
    sp.funcs[work->state](enemy, task);
    scratch->local.vx = 0;
    scratch->local.vy = 0;
    scratch->local.vz = 0;
    {
        SVECTOR  local;
        VECTOR   result;
        s32      flag;
        SVECTOR* localp = &local;

        walker   = &task->extra.tmd->coords[2];
        pos      = &scratch->local;
        local.vx = scratch->local.vx;
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
    enemy->bodyPos.vx = scratch->local.vx;
    enemy->bodyPos.vy = scratch->local.vy;
    enemy->bodyPos.vz = scratch->local.vz;
    enemy->coord      = &gGfxViewCoord;
    SCRATCH_STACK_RELEASE_BYTES(0x1C);
}
