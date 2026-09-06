#include "common.h"

#include "gameplay/1A8.h"
#include "gameplay/3CD8.h"

#include "main/gameflag.h"
#include "main/session.h"
#include "main/task.h"

extern s8 D_8007272D;

s32 func_acropolis_west_elevator_hall_8017F4C0(Task* task, s32 msgId, GpSaveLoc* src, GpSaveLoc* dst)
{
    *dst = *src;
    if (*(u16*)src == 1 && GameFlag_GetNibble(0x21) == 0 && src->field_5 == 0) {
        GameFlag_SetNibble(0x21, 1);
        D_8007272D   = 1;
        dst->field_2 = 7;
    }
    return 1;
}
