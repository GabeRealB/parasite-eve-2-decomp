#include "common.h"

#include <psyq/libcd.h>

INCLUDE_ASM("main/nonmatchings/E734", func_8001DF34);

INCLUDE_ASM("main/nonmatchings/E734", func_8001E2D4);

INCLUDE_ASM("main/nonmatchings/E734", func_8001E57C);

INCLUDE_ASM("main/nonmatchings/E734", func_8001E6AC);

bool E734_CDIsShellOpenBitSet(void)
{
    bool tmp;
    u8   result[8];

    // Writing it as (result[0] & CdlStatShellOpen) != 0 produces the wrong code
    CdControlB(CdlNop, NULL, result);
    tmp = result[0] & CdlStatShellOpen;
    return tmp != 0;
}

bool E734_CDCanIssueCommand(void)
{
    return CdDiskReady(1) == CdlComplete;
}
