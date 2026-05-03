#include "common.h"

#include <psyq/libcd.h>

INCLUDE_ASM("main/nonmatchings/E734", func_8001DF34);

INCLUDE_ASM("main/nonmatchings/E734", func_8001E2D4);

INCLUDE_ASM("main/nonmatchings/E734", func_8001E57C);

INCLUDE_ASM("main/nonmatchings/E734", func_8001E6AC);

INCLUDE_ASM("main/nonmatchings/E734", func_8001E7C0);

bool E734_CDCanIssueCommand()
{
    return CdDiskReady(1) == CdlComplete;
}
