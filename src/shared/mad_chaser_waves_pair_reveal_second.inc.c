/* Part of the scripted encounter library; see mad_chaser_waves.h. */

/// Forgets dead pair members, then reveals the second Sucklerceph after its delay.
///
/// Requires initialized pair work with the counter reset by the first reveal.
/// Borrowed enemies and their model tasks must remain live until forgotten.
/// A surviving second enemy is revealed on update 61 with the low spawn
/// halfword's command; a missing one skips the wait. Dispatch consumes the
/// stack request synchronously. Completion resets the counter and advances.
static void _overlayEncounterPairRevealSecond(Task* waveTask)
{
    enum { OVERLAY_ENCOUNTER_PAIR_REVEAL_WAIT_UPDATES = 60 };
    OverlayEncounterPairWork* work = waveTask->work;
    Enemy*                    enemy;
    Task*                     enemyTask;
    TmdObject*                model;
    ActorCommand              request;

    // Retain the partner only if the HP check leaves its borrowed pointer live.
    enemy = work->enemy1;
    _overlayEncounterForgetDeadPairMembers(waveTask);
    if (work->enemy1 != NULL) {
        if (++work->frames <= OVERLAY_ENCOUNTER_PAIR_REVEAL_WAIT_UPDATES) {
            return;
        }
        enemyTask                 = work->enemy1->task;
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
