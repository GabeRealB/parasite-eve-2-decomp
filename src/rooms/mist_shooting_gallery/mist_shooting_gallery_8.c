#include "common.h"

#include "main/display.h"
#include "main/gamemain.h"
#include "main/session.h"
#include "main/stage.h"
#include "main/task.h"
#include "main/ui.h"

/// The jukebox panel's descriptor; its update routine is the menu task
/// `func_mist_shooting_gallery_80180728`.
extern UiObjectDesc D_mist_shooting_gallery_8018535C;

/// Runs the jukebox panel over the room: takes the prim buffer and stops the
/// frame timer while the panel is up, waits for the panel to report closed,
/// then after ten more ticks gives both back, kills itself and ends the stage.
void func_mist_shooting_gallery_80180A00(Task* task)
{
    UiObject* obj;

    if (task->state == 0) {
        Stage_InitPrimBufOnce();
        obj = Ui_SpawnFromDesc(&D_mist_shooting_gallery_8018535C, task->spawnArg1, 1, 1, NULL);
        if (obj == NULL) {
            return;
        }
        GameMain_SetFrameTiming(0);
        gGameSession->uiOpen = 1;
        task->spawnArg2      = obj;
        task->state++;
    }

    if (task->state == 1) {
        obj = task->spawnArg2;
        if (obj->field_2E == -1 || obj->field_2E == 6) {
            Ui_TeardownTree(obj, obj->owner);
            task->killCountdown = 10;
            task->state         = 2;
        }
    }

    if (task->state == 2) {
        task->killCountdown--;
        if (task->killCountdown <= 0) {
            GameMain_SetFrameTiming(1);
            gGameSession->uiOpen = 0;
            taskKill(task);
            Stage_ReleasePrimBuf();
            Stage_SetEndingFlag();
        }
    }
}
