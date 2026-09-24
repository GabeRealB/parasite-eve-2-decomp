#include "common.h"

#include <psyq/libgte.h>

#include "main/task.h"
#include "rooms/shelter_b1_access_tunnel.h"

void func_shelter_b1_access_tunnel_8017DD08(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_shelter_b1_access_tunnel_8017D5F0;
    sp.funcs[task->state](task);
}
