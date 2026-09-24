#include "common.h"

#include "main/display.h"
#include "main/gamemain.h"
#include "main/session.h"
#include "main/stage.h"
#include "main/task.h"
#include "main/ui.h"

/// Descriptor of the panel `func_mist_parking_8017FF9C` opens.
extern UiObjectDesc D_mist_parking_80186590;

/// Opens the panel `D_mist_parking_80186590` with the task's `spawnArg1` as
/// its parameter, setting frame timing 0 and the session's UI flag while it is
/// open; once the panel reports -1 or 6 it is torn down, and ten frames later
/// frame timing 1 and the flag are restored and the task kills itself.
void func_mist_parking_8017FF9C(Task* task)
{
    UiObject* obj;

    if (task->state == 0) {
        Stage_InitPrimBufOnce();
        obj = Ui_SpawnFromDesc(&D_mist_parking_80186590, task->spawnArg1, 1, 1, NULL);
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
