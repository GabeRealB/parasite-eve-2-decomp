#include "common.h"

#include "main/display.h"
#include "main/gamemain.h"
#include "main/session.h"
#include "main/stage.h"
#include "main/task.h"
#include "main/ui.h"

extern UiObjectDesc D_shelter_1f_heliport_8018108C;

/// Opens the UI object described by `D_shelter_1f_heliport_8018108C` for the
/// task's `spawnArg1`, waits until the object reports state -1 or 6, tears it
/// down, and ten frames later restores the frame timing, releases the
/// primitive buffer and kills the task.
void func_shelter_1f_heliport_8017FDD4(Task* task)
{
    UiObject* obj;

    if (task->state == 0) {
        Stage_InitPrimBufOnce();
        obj = Ui_SpawnFromDesc(&D_shelter_1f_heliport_8018108C, task->spawnArg1, 1, 1, NULL);
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
