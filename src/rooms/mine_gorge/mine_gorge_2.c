#include "common.h"

#include "gameplay/1A8.h"
#include "gameplay/268.h"
#include "gameplay/3CD8.h"

#include "main/gameflag.h"
#include "main/session.h"
#include "main/task.h"

extern u8  D_80115768;
extern s32 D_mine_gorge_8017E2F0;
extern s32 D_mine_gorge_8017E500;
extern s32 D_mine_gorge_8017E610;

/// Cutscene gate on the forge's `0x13EF` direction message: when the payload's
/// direction byte is 1, flag nibble `0xC5` is still clear and the session is in
/// phase 1, raises the nibble and starts the script blob at
/// `D_mine_gorge_8017E610`.
s32 func_mine_gorge_8017D784(Task* task, s32 msgId, GpMsg13EF* arg2)
{
    u8 temp_s0 = arg2->field_2;

    if (temp_s0 == 1 && GameFlag_GetNibble(0xC5) == 0 && gGameSession->at4.loc.place == temp_s0) {
        GameFlag_SetNibble(0xC5, 1);
        func_800E8614((s32)&D_mine_gorge_8017E610, 0);
    }
    return 0;
}

s32 func_mine_gorge_8017D7F4(s32 arg0, s32 arg1, s32 arg2)
{
    if (arg2 == 0xA) {
        SndEvt_EnqueueType6(0x54050000 | arg2, 0, 0);
    }
    return 0;
}

/// Tears the mine gorge sequence down: the first pass raises `D_80115768` and
/// the session's `hideHud`, hides the display and starts the script blob pair
/// `D_mine_gorge_8017E2F0` / `D_mine_gorge_8017E500`; the next pass kills the
/// task and clears collection bit `0x11F`.
void func_mine_gorge_8017D828(Task* arg0)
{
    if (arg0->state == 0) {
        D_80115768 = 1;
        SetDispMask(0);
        gGameSession->hideHud = 1;
        func_800E8634((s32)&D_mine_gorge_8017E2F0, 0, (s32)&D_mine_gorge_8017E500);
    } else {
        taskKill(arg0);
        Gp_ClearCollectedBit(0x11F);
    }
    arg0->state = arg0->state + 1;
}
