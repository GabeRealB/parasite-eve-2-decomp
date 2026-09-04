#include "common.h"

#include "rooms/room_common.h"

/// Task exit callback for the save-prompt UI: if this task still owns
/// `Wip_UiHolder`, clear it, then free the spawned UI object and kill the task.
void Room_SaveUi01(Task* task)
{
    WipUiHolder* holder;

    holder = task->spawnArg2;
    if (Wip_UiHolder == holder) {
        Wip_UiHolder = NULL;
    }
    Ui_FreeAndKill(task);
}
