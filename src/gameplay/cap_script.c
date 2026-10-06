#include "gameplay/captions.h"

#include "types.h"

#include "captions.h"
#include "gameplay/evs_scripts.h"
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
        func_800E44A0(task);
    }
}

void Gp_EndWaitTask(Task* task)
{
    CapActionRequest* request;

    request = task->spawnArg2.pointer;
    switch (task->state) {
        case 0:
            Task_Spawn(1, 0x2C, 0, request);
            task->state++;
            break;
        case 1:
            if (request->done != 0) {
                stageRequestModeTaskExit();
                taskKill(task);
            }
            break;
    }
}
