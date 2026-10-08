/* Part of the scripted encounter library; see mad_chaser_waves.h. */

/// Reveals the first Sucklerceph and starts the delay before revealing its partner.
///
/// Requires initialized pair work; non-NULL enemies and their model tasks must
/// be live. The low spawn halfword is the reveal command, including its spot
/// index. Dispatch borrows the stack request only until it returns. A missing
/// first enemy still resets the update counter and advances the spawner state.
static void _overlayEncounterPairRevealFirst(Task* waveTask)
{
    OverlayEncounterPairWork* work = waveTask->work;
    Enemy*                    enemy;
    Task*                     enemyTask;
    TmdObject*                model;
    ActorCommand              request;

    enemy = work->enemy0;
    if (enemy != NULL) {
        enemyTask                 = enemy->task;
        model                     = enemyTask->extra.tmd;
        model->texturePageOffset  = OVERLAY_ENCOUNTER_PAIR_TEXTURE_PAGE_OFFSET;
        model->clutRowOffset      = OVERLAY_ENCOUNTER_PAIR_CLUT_ROW_OFFSET;
        enemy->workType           = ENEMY_WORK_PLAIN;
        request.context.loc.stage = OVERLAY_ENCOUNTER_PAIR_COMMAND_STAGE;
        request.context.loc.area  = OVERLAY_ENCOUNTER_PAIR_COMMAND_AREA;
        request.command           = waveTask->spawnArg1.value;
        TASK_MESSAGE_DISPATCH_POINTER(enemyTask, ACTOR_COMMAND_MESSAGE_APPLY, &request, 0);
    }
    work->frames = 0;
    waveTask->state++;
}
