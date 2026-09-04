#include "common.h"

#include "gameplay/268.h"
#include "gameplay/3CD8.h"
#include "main/gameflag.h"

s32 Room_Script17(s32 arg0, s32 arg1, s32 arg2)
{
    if (arg2 != 8) {
        if (arg2 == 0xD) {
            if (GameFlag_GetNibble(0x11B) >= 2) {
                if (func_800B7420(0x83) == 0) {
                    Gp_RunCapCmd1(0xE);
                } else {
                    Gp_RunCapCmd1(4);
                }
            } else {
                Gp_RunCapCmd1(0xD);
            }
        }
    }
    return 0;
}
