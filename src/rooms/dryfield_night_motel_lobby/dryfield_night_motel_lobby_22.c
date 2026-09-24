#include "common.h"

#include "main/task.h"

#include "rooms/dryfield_night_motel_lobby.h"
#include "rooms/room_common.h"

void func_dryfield_night_motel_lobby_80180D08(Task* task)
{
    TaskFunc states[2] = { func_dryfield_night_motel_lobby_80181298, func_dryfield_night_motel_lobby_801807C0 };

    states[task->state](task);
}
