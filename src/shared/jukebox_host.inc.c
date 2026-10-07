/* Part of the jukebox library; see jukebox.h. */

/// Runs the jukebox panel over the room: takes the prim buffer and stops the
/// frame timer while the panel is up, waits for the panel to report closed,
/// then after ten more ticks gives both back, kills itself and ends the stage.
void jukeboxHostTask(Task* task)
{
    UiObject* obj;

    if (task->state == 0) {
        stageEnsureHeapTaskPrimitiveBuffer();
        obj = uiSpawnObject(&gJukeboxPanelDesc, task->spawnArg1, 1, 1, NULL);
        if (obj == NULL) {
            return;
        }
        displaySetFrameTiming(DISPLAY_TIMING_EVERY_VBLANK);
        gGameSession->uiOpen    = 1;
        task->spawnArg2.pointer = obj;
        task->state++;
    }

    if (task->state == 1) {
        obj = task->spawnArg2.pointer;
        if (obj->result == USER_INTERFACE_RESULT_CANCEL || obj->result == USER_INTERFACE_RESULT_CONFIRM) {
            uiStartTreeClosing(obj, obj->owner);
            task->killCountdown = 10;
            task->state         = 2;
        }
    }

    if (task->state == 2) {
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
