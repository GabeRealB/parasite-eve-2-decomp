#include "common.h"

#include "gameplay/268.h"
#include "gameplay/3CD8.h"
#include "main/gameflag.h"

/// Message-table handler for message 0x13F0. On event 0xD it runs a CAP
/// command: 0xD while event nibble 0x11B is below 2, otherwise 4 or 0xE
/// depending on whether `func_800B7420(0x83)` reports non-zero. Every other
/// event does nothing. Always answers 0.
s32 func_dryfield_night_cellar_8017D5D0(s32 arg0, s32 arg1, s32 arg2)
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
