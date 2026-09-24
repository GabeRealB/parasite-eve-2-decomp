#include "common.h"

#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"

#include "main/gameflag.h"
#include "main/task.h"

extern s8  D_801153F1;
extern s16 D_800691CA;

extern s32 D_shelter_b6_corridor_8017F354;
extern s32 D_shelter_b6_corridor_8017F684;

void func_shelter_b6_corridor_8017E19C(s32 arg0)
{
    if (!(gGameSession->flowFlags & 0x80)) {
        gGameSession->flowFlags |= 0x80;
        D_801153F1               = arg0;
        Gp_ReleaseStateF0Add((GpObj20E*)Gp_LookupSlot4(1), 0x31);
        Task_CallExit((Task*)Gp_LookupSlot4(1));
    }
}
