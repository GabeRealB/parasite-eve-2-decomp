#include "gameplay/captions.h"

#include "types.h"

#include "captions.h"
#include "gameplay/evs_scripts.h"
#include "gameplay/item_menu.h"
#include "gameplay/message.h"

#include "main/session.h"
#include "main/stage.h"
#include "main/task.h"

void capPlaybackTask(Task* task)
{
    enum { CAP_PLAYBACK_INITIAL_STATE             = 0,
           CAP_ACTION_CAPTURE_ROOM_EFFECT_MESSAGE = 3000,
           CAP_ACTION_CAPTURE_ENTER               = 1 };

    if (D_801156F9 == 0) {
        switch (task->state) {
            case CAP_PLAYBACK_INITIAL_STATE:
                // Notify the room receiver before the first action-capture playback update.
                if (D_80115666 == CAP_PLAYBACK_ACTION_CAPTURE) {
                    taskMessageDispatch(gameGetTaskSlot(GAME_TASK_SLOT_ROOM_EFFECT), CAP_ACTION_CAPTURE_ROOM_EFFECT_MESSAGE, CAP_ACTION_CAPTURE_ENTER, 0);
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
