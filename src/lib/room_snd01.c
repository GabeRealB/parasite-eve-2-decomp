#include "common.h"

#include "gameplay/3CD8.h"

#include "main/task.h"

s32 Room_Snd01(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    switch (arg2) {
        case 0x8:
            Gp_EnqueueStageSnd6(0x52020008, 0, 0);
            break;
        case 0x9:
            Gp_EnqueueStageSnd6(0x52020009, 0, 0);
            /* fallthrough */
        case 0xC:
            Gp_EnqueueStageSnd6(0x5202000C, 0, 0);
            break;
        case 0x65:
            if (Gp_GetCapEventKey() == 1) {
                Gp_EnqueueStageSnd6(0x5202000D, 0, 0);
            }
            break;
        case 0x78:
            if (Gp_GetCapEventKey() == 0) {
                Gp_EnqueueStageSnd6(0x5202000D, 0, 0);
            }
            break;
    }
    return 0;
}
