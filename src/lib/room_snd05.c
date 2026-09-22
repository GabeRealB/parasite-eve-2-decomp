#include "common.h"

#include "gameplay/3CD8.h"

#include "main/gameflag.h"
#include "main/task.h"

s32 Room_Snd05(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    switch (arg2) {
        case 7:
            Gp_EnqueueStageSnd6(0x52170007, 0, 0);
            break;
        case 21:
            Gp_EnqueueStageSnd6(0x52170015, 0, 0);
            GameFlag_SetNibble(0x4A, 2);
            break;
    }
    return 0;
}
