#include "common.h"
#include "gameplay/3CD8.h"
#include "main/gameflag.h"
#include "main/task.h"
#include "gameplay/1A8.h"
#include "main/sound.h"

s32 func_dryfield_night_factory_80180914(Task* task, s32 msgId, s32 arg2, s32 arg3)
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

s32 func_dryfield_night_factory_80180980(Task* task, s32 msgId, GpMsg13EF* arg2)
{
    if ((arg2->field_2 == 1) && (GameFlag_GetNibble(0x2C) == 0)) {
        Gp_SpawnIfCapIdle(0xB, 1);
        GameFlag_SetNibble(0x2C, 1);
        func_800E3FAC(0xA2, 0xA);
        SndEvt_EnqueueType6(0x5217000A, 0, 0);
    }
    return 0;
}
