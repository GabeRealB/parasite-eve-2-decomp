#include "common.h"

#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"

s32 func_dryfield_cellar_8017D6F4(void)
{
    return 0;
}

s32 func_dryfield_cellar_8017D6FC(s32 arg0, s32 arg1, s32 arg2)
{
    if (arg2 == 3) {
        SndEvt_EnqueueType6(0x52220000 | 3, 0, 0);
    }
    return 0;
}
