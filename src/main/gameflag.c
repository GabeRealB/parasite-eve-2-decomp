#include "common.h"

#include "main/gameflag.h"
#include "main/mc.h"
#include "main/pad.h"

void GameFlag_SetNibble(s32 arg0, s32 arg1)
{
    s32          idx;
    u8*          ptr;
    register s32 val asm("v1");
    register s32 nibble asm("a0");

    idx = arg0 / 2;
    if (arg0 & 1) {
        ptr    = &D_80073980[idx];
        nibble = arg1 & 0xF;
        val    = ptr[4] & 0xF0;
    } else {
        ptr    = &D_80073980[idx];
        nibble = arg1 << 4;
        val    = ptr[4] & 0xF;
    }
    ptr[4] = val | nibble;
}

s32 GameFlag_GetNibble(s32 arg0)
{
    s32 idx;

    idx = arg0 / 2;
    if (arg0 & 1) {
        return D_80073980[idx + 4] & 0xF;
    }
    return D_80073980[idx + 4] >> 4;
}

s32 Pad_CheckFlag800(void)
{
    return Pad_CheckButtons(0, 1, 0x800);
}
