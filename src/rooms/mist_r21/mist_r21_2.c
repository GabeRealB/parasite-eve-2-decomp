#include "common.h"
#include "gameplay/1A8.h"

#include "main/task.h"

/// Message-table handler for id 0x13F1: accepts the message and does nothing.
s32 func_mist_r21_8017D5DC(void)
{
    return 0;
}

/// Message-table handler for id 0x13EE: copies the location record it is given
/// onto the reply record unchanged and answers 1.
s32 func_mist_r21_8017D5E4(Task* task, s32 msgId, GpSaveLoc* src, GpSaveLoc* dst)
{
    *dst = *src;
    return 1;
}

/// Message-table handler for id 0x13F0: accepts the message and does nothing.
s32 func_mist_r21_8017D60C(void)
{
    return 0;
}

/// Message-table handler for id 0x13EF: accepts the message and does nothing.
s32 func_mist_r21_8017D614(void)
{
    return 0;
}
