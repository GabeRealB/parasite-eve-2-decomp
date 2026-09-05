#include "common.h"
#include "main/task.h"
#include "gameplay/3A34.h"
extern TaskDesc D_mine_mesa_80189B2C;
extern Task*    D_mine_mesa_80189B4C;
extern GpObj4A  D_mine_mesa_801890EC[4];

void func_mine_mesa_8017DDF0(void)
{
    if (D_mine_mesa_80189B4C == NULL) {
        D_mine_mesa_80189B4C = Task_SpawnFromTable(&D_mine_mesa_80189B2C, 0, 0, 0);
    }
}
