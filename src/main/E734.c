#include "common.h"

#include <psyq/libcd.h>

#include "main/game.h"

INCLUDE_ASM("main/nonmatchings/E734", func_8001DF34);

INCLUDE_ASM("main/nonmatchings/E734", func_8001E2D4);

INCLUDE_ASM("main/nonmatchings/E734", func_8001E57C);

s16 func_8001E57C(void);
s16 E734_CDIsShellOpenBitSet(void);

s32 func_8001E6AC(s32 arg0, s32 arg1)
{
    GStruct3* state;
    s32       status;
    u16       a1;

    state = &D_80068FA0;
    switch (state->field_228) {
    case 0:
        status = CdSync(1, NULL);
        switch (status) {
        case CdlNoIntr:
            break;
        case CdlComplete:
            state->field_1d4 = 0;
            return 1;
        case CdlDiskError:
            state->field_1d4 = 1;
            if (E734_CDIsShellOpenBitSet() != 0) {
                state->field_228++;
                break;
            }
            a1 = arg1 & 0xFFFF;
            if (a1 == 0) {
                return 2;
            }
            if ((arg0 & 0xFFFF) == a1) {
                return 1;
            }
            return 3;
        default:
            return 2;
        }
        break;
    case 1:
        if (func_8001E57C() != 0) {
            state->field_228 = 0;
            return 2;
        }
        break;
    default:
        return 0;
    }
    return 0;
}

s16 E734_CDIsShellOpenBitSet(void)
{
    s16 tmp;
    u8  result[8];

    // Writing it as (result[0] & CdlStatShellOpen) != 0 produces the wrong code
    CdControlB(CdlNop, NULL, result);
    tmp = result[0] & CdlStatShellOpen;
    return tmp != 0;
}

bool E734_CDCanIssueCommand(void)
{
    return CdDiskReady(1) == CdlComplete;
}
