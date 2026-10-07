/* Part of the Desert Chaser library; see desert_chaser.h. */

/// Updates the cutscene chaser's lighting, behavior state and view-space body position.
///
/// Requires live enemy/model/work and a work state indexing the carrier's
/// four-entry behavior table. The state handler runs with scratch storage
/// reserved; stateEntered marks a change since the previous call. Afterwards
/// part 2's origin becomes Enemy::bodyPos relative to the view coordinate.
static void _desertChaserFrameState(Enemy* enemy, Task* task)
{
    DesertChaserWork*         work;
    EnemyTaskFuncTable4       states;
    DesertChaserFrameScratch* scratch;

    work = task->work;
    // The player-slot query is retained although its result is unused.
    gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    states                                = gDesertChaserStates;
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    scratch                               = SCRATCH_STACK_RESERVE_BLOCK(DesertChaserFrameScratch);
    // Relight from the model root before dispatching behavior.
    actorRenderComposeCoord(task->extra.tmd->coords);
    scratch->rootPos.vx = task->extra.tmd->coords->workm.t[0];
    scratch->rootPos.vy = task->extra.tmd->coords->workm.t[1];
    scratch->rootPos.vz = task->extra.tmd->coords->workm.t[2];
    worldCoordUpdateActorColor(enemy, &scratch->rootPos, 0, 0);
    if (work->prevState != work->state) {
        work->stateEntered = 1;
    } else {
        work->stateEntered = 0;
    }
    work->prevState = work->state;
    states.funcs[work->state](enemy, task);
    scratch->bodyPos.vx = 0;
    scratch->bodyPos.vy = 0;
    scratch->bodyPos.vz = 0;
    // Publish the neck-base origin in view space after the state has moved it.
    actorTransformToView(&task->extra.tmd->coords[2], &scratch->bodyPos);
    enemy->bodyPos.vx = scratch->bodyPos.vx;
    enemy->bodyPos.vy = scratch->bodyPos.vy;
    enemy->bodyPos.vz = scratch->bodyPos.vz;
    enemy->coord      = &gGfxViewCoord;
    SCRATCH_STACK_RELEASE_BLOCK(DesertChaserFrameScratch);
}
