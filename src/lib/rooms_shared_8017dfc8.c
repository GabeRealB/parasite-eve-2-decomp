#include "common.h"

#include "gameplay/268.h"
#include "gameplay/3CD8.h"

#include "main/gameflag.h"
#include "main/task.h"

extern TaskDesc Room_Script14Desc;

s32 RoomsShared8017dfc8(s32 arg0, s32 arg1, s32 arg2)
{
    if (arg2 == 1) {
        if (GameFlag_GetNibble(0x7B) >= 2) {
            Gp_AgeFlag119();
            if (Gp_HasCollectedBit(0x119) == 0) {
                Gp_SetCurBit2Flag(0x1B, 1);
            }
            Gp_SpawnIfCapIdle(1, 1);
            Task_SpawnFromTable(&Room_Script14Desc, 0, 0, 0);
        } else {
            Gp_SpawnIfCapIdle(0x14, 1);
        }
    }
    return 0;
}
