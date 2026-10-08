/* Part of the scripted encounter library; see mad_chaser_waves.h. */

/// Reveals a spawned Slouch after the encounter's sixty-update delay.
///
/// Requires single-enemy work with a live enemy/model and a zero-based update
/// counter. The sixty-first call installs its palette, enables ordinary enemy
/// work, sends the low spawn halfword as the emerge command in the synthetic
/// stage-0/area-0x2A context, and advances the wave task. Dispatch borrows the
/// stack request only for the call; the wave task does not own the enemy.
static void _overlayEncounterRevealSlouch(Task* waveTask)
{
    enum {
        OVERLAY_ENCOUNTER_SLOUCH_REVEAL_DELAY        = 60,
        OVERLAY_ENCOUNTER_SLOUCH_TEXTURE_PAGE_OFFSET = 2,
        OVERLAY_ENCOUNTER_SLOUCH_CLUT_ROW_OFFSET     = 4,
        OVERLAY_ENCOUNTER_SLOUCH_COMMAND_STAGE       = 0,
        OVERLAY_ENCOUNTER_SLOUCH_COMMAND_AREA        = 0x2A
    };
    OverlayEncounterSingleWork* work = waveTask->work;
    Enemy*                      enemy;
    Task*                       enemyTask;
    TmdObject*                  model;
    ActorCommand                request;

    enemy     = work->enemy;
    enemyTask = enemy->task;
    if (++work->frames > OVERLAY_ENCOUNTER_SLOUCH_REVEAL_DELAY) {
        model                     = enemyTask->extra.tmd;
        model->texturePageOffset  = OVERLAY_ENCOUNTER_SLOUCH_TEXTURE_PAGE_OFFSET;
        model->clutRowOffset      = OVERLAY_ENCOUNTER_SLOUCH_CLUT_ROW_OFFSET;
        enemy->workType           = ENEMY_WORK_PLAIN;
        request.context.loc.stage = OVERLAY_ENCOUNTER_SLOUCH_COMMAND_STAGE;
        request.context.loc.area  = OVERLAY_ENCOUNTER_SLOUCH_COMMAND_AREA;
        request.command           = waveTask->spawnArg1.value;
        TASK_MESSAGE_DISPATCH_POINTER(enemyTask, ACTOR_COMMAND_MESSAGE_APPLY, &request, 0);
        waveTask->state++;
    }
}
