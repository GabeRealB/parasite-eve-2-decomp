#include "common.h"
#include "main/gameflag.h"
#include "main/task.h"

void func_mine_secret_passage_8017D914(Task* arg0)
{
    if (GameFlag_GetNibble(0x172) == 0) {
        GameFlag_SetNibble(0x172, 1);
        Gp_SpawnIfCapIdle(3, 1);
    }
    arg0->state = (s32)(arg0->state + 1);
}

void func_mine_secret_passage_8017D968(void)
{
}
