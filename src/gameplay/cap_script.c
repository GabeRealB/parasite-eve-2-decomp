#include "gameplay/captions.h"

#include "types.h"

#include "captions.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/item_menu.h"
#include "gameplay/message.h"

#include "main/session.h"
#include "main/stage.h"
#include "main/task.h"

void func_800E70AC(Task* task)
{
    if (D_801156F9 == 0) {
        switch (task->state) {
            case 0:
                if (D_80115666 == 2) {
                    taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_ROOM_EFFECT), 0xBB8, 1, 0);
                }
                task->state++;
                break;
        }
        capUpdatePlaybackTask(task);
    }
}

void capActionPromptExitTask(Task* task)
{
    enum { CAP_ACTION_PROMPT_START = 0,
           CAP_ACTION_PROMPT_WAIT  = 1 };
    CapActionRequest* request;

    request = task->spawnArg2.pointer;
    switch (task->state) {
        case CAP_ACTION_PROMPT_START:
            taskSpawn(ITEM_PICKUP_ACTION_TASK_BANK, ITEM_PICKUP_ACTION_TASK_TYPE, 0, request);
            task->state++;
            break;
        case CAP_ACTION_PROMPT_WAIT:
            if (request->done != 0) {
                stageRequestModeTaskExit();
                taskKill(task);
            }
            break;
    }
}
