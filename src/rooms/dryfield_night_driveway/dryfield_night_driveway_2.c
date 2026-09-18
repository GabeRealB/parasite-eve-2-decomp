#include "common.h"

#include "gameplay/3CD8.h"
#include "main/session.h"
#include "main/task.h"

extern s32 D_dryfield_night_driveway_8017F7A4;
extern s32 D_dryfield_night_driveway_8017F998;
extern s32 D_dryfield_night_driveway_8017FB00;

s32 func_dryfield_night_driveway_8017DCE4(void)
{
    return 0;
}

s32 func_dryfield_night_driveway_8017DCEC(void)
{
    return 0;
}

s32 func_dryfield_night_driveway_8017DCF4(void)
{
    return 0;
}

void func_dryfield_night_driveway_8017DCFC(Task* arg0)
{
    arg0->field_24 = &D_dryfield_night_driveway_8017F7A4;
    Game_SetPtrSlot(arg0, 7);
    if ((Game_GetPtrSlot(0xA) != 0) && (gGameSession->field_8 == 4)) {
        func_800E8634((s32)&D_dryfield_night_driveway_8017FB00, 0, (s32)&D_dryfield_night_driveway_8017F998);
    }
    arg0->state = (s32)(arg0->state + 1);
}
