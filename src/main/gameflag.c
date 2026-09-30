#include "main/gameflag.h"

#include "types.h"

#include "main/pad.h"
#include "main/task.h"
#include "task.h"
#include "main/task_types.h"

#include "gameplay/captions.h"
#include "gameplay/evs_scripts.h"

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

void GameFlag_SetNibble(s32 index, s32 value)
{
    s32 idx;

    idx = index / 2;
    if (index & 1) {
        GameFlag_NibbleBanks[0].payload.packedFlags[idx] = (GameFlag_NibbleBanks[0].payload.packedFlags[idx] & 0xF0) | (value & 0xF);
    } else {
        GameFlag_NibbleBanks[0].payload.packedFlags[idx] = (GameFlag_NibbleBanks[0].payload.packedFlags[idx] & 0xF) | (value << 4);
    }
}

s32 GameFlag_GetNibble(s32 index)
{
    s32 idx;

    idx = index / 2;
    if (index & 1) {
        return GameFlag_NibbleBanks[0].payload.packedFlags[idx] & 0xF;
    }
    return GameFlag_NibbleBanks[0].payload.packedFlags[idx] >> 4;
}

s32 Pad_CheckFlag800(void)
{
    return Pad_CheckButtons(0, 1, 0x800);
}
