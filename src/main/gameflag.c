#include "common.h"

#include "main/gameflag.h"
#include "main/mc.h"
#include "main/pad.h"
#include "main/task.h"
#include "gameplay/3CD8.h"

TaskDesc D_80067734[] = {
    { 0x0, 0xC0, taskKill },
    { 0x0, 0xC0, taskKill },
    { 0x0, 0xC0, taskKill },
    { 0x0, 0xC0, taskKill },
    { 0x0, 0xC0, NULL },
    { 0x0, 0x20, taskKill },
    { 0x0, 0x80, func_800E7570 },
    { 0x0, 0x20, func_800E8830 },
    { 0x0, 0x80, func_800E8888 },
    { 0x0, 0xC0, taskKill },
    { 0x0, 0x20, taskKill },
    { 0x0, 0x80, Gp_EndWaitTask },
    { 0x0, 0x20, Gp_ShakeTask },
    { 0x0, 0x20, Gp_VolFadeTask },
    { 0x0, 0x20, Gp_SndFadeTask },
    { 0x0, 0xC0, NULL },
    { 0x0, 0xC0, NULL },
    { 0x0, 0x20, func_800E4028 },
    { 0xFFFF, 0x20, NULL },
};

void GameFlag_SetNibble(s32 arg0, s32 arg1)
{
    s32 idx;

    idx = arg0 / 2;
    if (arg0 & 1) {
        u8* ptr = &D_80073980[idx];

        ptr[4] = (ptr[4] & 0xF0) | (arg1 & 0xF);
    } else {
        u8* ptr = &D_80073980[idx];

        ptr[4] = (ptr[4] & 0xF) | (arg1 << 4);
    }
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
