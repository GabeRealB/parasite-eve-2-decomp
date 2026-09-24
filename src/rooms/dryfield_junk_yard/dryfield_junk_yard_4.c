#include "common.h"

#include "gameplay/268.h"
#include "gameplay/3CD8.h"
#include "main/gameflag.h"

/// Handler for message 0x13F0 in the room's message table, keyed by `arg2`.
/// Point 6 plays CAP command 0xC until nibble 0x3A is set, and 6 after. Point 8
/// plays command 9 unless bit flag 0x1C is set; with it set, command 8 plays
/// only while nibble 0x73 is still clear and 0x7C is set, and otherwise the
/// point's own CAP slot starts. Always returns 0.
s32 func_dryfield_junk_yard_8017D994(s32 arg0, s32 arg1, s32 arg2)
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
