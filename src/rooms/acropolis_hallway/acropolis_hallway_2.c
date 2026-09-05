#include "common.h"

#include "gameplay/268.h"
#include "gameplay/4CC.h"
#include "gameplay/D4.h"

#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"

s32 func_acropolis_hallway_8017D734(s32 arg0, s32 arg1, s32 arg2)
{
    switch (arg2) { /* irregular */
        case 6:
            SndEvt_EnqueueType6(0x51070006, 0, 0);
            break;
        case 7:
            SndEvt_EnqueueType6(0x51070007, 0, 0);
            break;
    }
    return 0;
}
