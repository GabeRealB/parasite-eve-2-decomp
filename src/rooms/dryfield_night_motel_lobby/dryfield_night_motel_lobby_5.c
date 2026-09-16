#include "common.h"

#include "main/task.h"

#include "rooms/dryfield_night_motel_lobby.h"

void func_dryfield_night_motel_lobby_80180D58(Task* task)
{
    TaskFuncTable11 states;

    states = D_dryfield_night_motel_lobby_8017D6B0;
    states.funcs[task->state](task);
}
