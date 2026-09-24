#include "common.h"

#include "main/task.h"
#include "rooms/mine_mesa.h"

void func_mine_mesa_8017E8FC(s32 arg0)
{
    if (D_mine_mesa_80189B5C != NULL) {
        D_mine_mesa_80189B5C->state = arg0;
    }
}
