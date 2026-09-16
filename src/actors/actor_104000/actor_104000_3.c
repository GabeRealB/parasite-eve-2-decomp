#include "common.h"
#include "main/task.h"

s32 func_actor_104000_80138524(Task* arg0)
{
    s16* temp_a0;

    temp_a0 = arg0->idMap;
    if (*temp_a0 == 0xB) {
        *temp_a0 = 0xD;
    }
    return 1;
}
