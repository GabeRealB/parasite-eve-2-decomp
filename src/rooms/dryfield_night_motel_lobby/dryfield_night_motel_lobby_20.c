#include "common.h"

#include "main/task.h"
#include "rooms/dryfield_night_motel_lobby.h"

void func_dryfield_night_motel_lobby_8017FE38(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_dryfield_night_motel_lobby_8017D6A4;
    sp.funcs[task->state](task);
}
