#include "common.h"

#include "gameplay/268.h"
#include "gameplay/3CD8.h"
#include "main/gameflag.h"

s32 RoomsShared8017d994(s32 arg0, s32 arg1, s32 arg2)
{
    switch (arg2) {
        case 6:
            Gp_RunCapCmd1(GameFlag_GetNibble(0x3A) <= 0 ? 0xC : 6);
            break;
        case 8:
            SOFT_TOUCH_REG(arg2);
            if (Gp_GetCurBit2Flag(0x1C) == 1) {
                if (GameFlag_GetNibble(0x73) != 0) {
                    Gp_StartCapSlot(arg2, 1, 0);
                } else if (GameFlag_GetNibble(0x7C) != 0) {
                    Gp_RunCapCmd1(8);
                } else {
                    Gp_StartCapSlot(arg2, 1, 0);
                }
            } else {
                Gp_RunCapCmd1(9);
            }
            break;
    }
    return 0;
}
