/* Part of the dryfield driveway library; see dryfield_driveway.h. */

/// Starts the driveway encounter script and clears its map mark when the event ends.
///
/// Entry holds scripted player control and starts CAP command 1. One intervening
/// tick precedes the event script; its start tick also tests completion. The task
/// waits for the global event state to clear, then clears the current area's map
/// mark and kills itself. The carrier's script/resources must remain live; the
/// script owns playback and control restoration.
static void _drivewayCutsceneTask(Task* task)
{
    enum {
        DRIVEWAY_CUTSCENE_START       = 0,
        DRIVEWAY_CUTSCENE_DELAY       = 1,
        DRIVEWAY_CUTSCENE_RUN_SCRIPT  = 2,
        DRIVEWAY_CUTSCENE_WAIT_EVENT  = 3,
        DRIVEWAY_CUTSCENE_CAP_COMMAND = 1,
    };
    s32 state;

    state = task->state;
    switch (state) {
        case DRIVEWAY_CUTSCENE_START:
            playerActorSetScriptedControl(GAME_ACTOR_SCRIPTED_CONTROL_HOLD);
            capRunCommandWithTransition(DRIVEWAY_CUTSCENE_CAP_COMMAND);
            task->state += 1;
            return;
        case DRIVEWAY_CUTSCENE_DELAY:
            task->state = DRIVEWAY_CUTSCENE_RUN_SCRIPT;
            return;
        case DRIVEWAY_CUTSCENE_RUN_SCRIPT:
            evsStartScript(gDrivewayCutsceneScript, EVENT_SCRIPT_HUD_HIDE_RESTORE);
            task->state += 1;
            /* fallthrough */
        case DRIVEWAY_CUTSCENE_WAIT_EVENT:
            if (gGameSession->eventState == 0) {
                areaClearMapMark(&gGameSession->location.loc);
                taskKill(task);
            }
            return;
    }
}
