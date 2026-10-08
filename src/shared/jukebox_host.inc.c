/* Part of the jukebox library; see jukebox.h. */

/// Hosts the jukebox panel and returns the stage mode after its closing delay.
///
/// Requires the carrier's panel descriptor and live stage/session state.
/// Forwards `spawnArg1` as panel content, borrows the UI object in `spawnArg2`
/// and uses the stage's heap primitive buffer. Spawn failure retries in state 0.
/// A successful panel switches to one-vblank timing and marks UI open. Confirm
/// or cancel starts tree closing and a ten-tick countdown, including that tick;
/// expiry restores two-vblank timing, releases the buffer and requests mode exit.
static void _jukeboxHostTask(Task* task)
{
    enum {
        JUKEBOX_HOST_OPEN         = 0,
        JUKEBOX_HOST_WAIT         = 1,
        JUKEBOX_HOST_CLOSING      = 2,
        JUKEBOX_HOST_CLOSE_TICKS  = 10,
        JUKEBOX_HOST_CONTROL_MODE = 1,
        JUKEBOX_HOST_OPEN_DELAY   = 1
    };
    UiObject* panel;

    if (task->state == JUKEBOX_HOST_OPEN) {
        stageEnsureHeapTaskPrimitiveBuffer();
        panel = uiSpawnObject(&gJukeboxPanelDesc, task->spawnArg1, JUKEBOX_HOST_CONTROL_MODE, JUKEBOX_HOST_OPEN_DELAY, NULL);
        if (panel == NULL) {
            return;
        }
        displaySetFrameTiming(DISPLAY_TIMING_EVERY_VBLANK);
        gGameSession->uiOpen    = 1;
        task->spawnArg2.pointer = panel;
        task->state++;
    }

    if (task->state == JUKEBOX_HOST_WAIT) {
        panel = task->spawnArg2.pointer;
        if (panel->result == USER_INTERFACE_RESULT_CANCEL || panel->result == USER_INTERFACE_RESULT_CONFIRM) {
            uiStartTreeClosing(panel, panel->owner);
            task->killCountdown = JUKEBOX_HOST_CLOSE_TICKS;
            task->state         = JUKEBOX_HOST_CLOSING;
        }
    }

    // Closing consumes its first tick in the same call that requested it.
    if (task->state == JUKEBOX_HOST_CLOSING) {
        task->killCountdown--;
        if (task->killCountdown <= 0) {
            displaySetFrameTiming(DISPLAY_TIMING_TWO_VBLANKS);
            gGameSession->uiOpen = 0;
            taskKill(task);
            stageReleaseTaskPrimitiveBuffer();
            stageRequestModeTaskExit();
        }
    }
}
