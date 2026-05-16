#include "common.h"

#include <psyq/libetc.h>
#include <psyq/libcd.h>

#include "main/unknown_syms.h"
#include "main/fs.h"

INCLUDE_ASM("main/nonmatchings/12D18", func_80022518);

INCLUDE_ASM("main/nonmatchings/12D18", func_8002252C);

INCLUDE_ASM("main/nonmatchings/12D18", func_80022598);

INCLUDE_ASM("main/nonmatchings/12D18", func_800225D4);

INCLUDE_ASM("main/nonmatchings/12D18", func_8002265C);

INCLUDE_ASM("main/nonmatchings/12D18", func_80022BD0);

INCLUDE_ASM("main/nonmatchings/12D18", func_80022CF0);

INCLUDE_ASM("main/nonmatchings/12D18", func_800231A8);

INCLUDE_ASM("main/nonmatchings/12D18", func_8002362C);

INCLUDE_ASM("main/nonmatchings/12D18", func_80023748);

INCLUDE_ASM("main/nonmatchings/12D18", func_8002397C);

INCLUDE_ASM("main/nonmatchings/12D18", F12D18_InitStage0TablesCb);

INCLUDE_ASM("main/nonmatchings/12D18", func_80023FA0);

INCLUDE_ASM("main/nonmatchings/12D18", func_800246B0);

INCLUDE_ASM("main/nonmatchings/12D18", func_800248B4);

INCLUDE_ASM("main/nonmatchings/12D18", func_80024A28);

INCLUDE_ASM("main/nonmatchings/12D18", func_80024DB8);

INCLUDE_ASM("main/nonmatchings/12D18", func_80024EC0);

INCLUDE_ASM("main/nonmatchings/12D18", func_80024FEC);

INCLUDE_ASM("main/nonmatchings/12D18", func_80025170);

INCLUDE_ASM("main/nonmatchings/12D18", func_8002526C);

INCLUDE_ASM("main/nonmatchings/12D18", func_80025338);

INCLUDE_ASM("main/nonmatchings/12D18", func_80025408);

INCLUDE_ASM("main/nonmatchings/12D18", func_8002548C);

void F12D18_InitStage0Tables(void)
{
    CdlLOC headerPos;

    // Reset all tables.
    D5B498_8006C228            = 0;
    D5B498_Stage0FileTableLen  = 0;
    D5B498_Stage0FileTable2Len = 0;
    D5B498_Stage0FileTable4Len = 0;
    D5B498_Stage0FileTable1Len = 0;
    D5B498_Stage0FileTable3Len = 0;

    // Read the stage header.
    D5B498_ReqCdSector = D5B498_Stage0HdrSect;
    CdIntToPos(D5B498_Stage0HdrSect, &headerPos);
    CdControlF(CdlReadN, &headerPos.minute);
    CdReadyCallback(F12D18_InitStage0TablesCb);
    D5B498_CurrVBlank = VSync(-1);
}

INCLUDE_ASM("main/nonmatchings/12D18", func_80025580);

INCLUDE_ASM("main/nonmatchings/12D18", func_8002563C);

INCLUDE_ASM("main/nonmatchings/12D18", func_800256A8);

void F12D18_800256F4(u8 arg0)
{
    D5B498_CdErrorCount += 1;
    CdReadyCallback(NULL);
    CdSyncCallback(NULL);

    if (arg0 == C12D18_800256F4_ARG_2) {
        D5B498_8006C228 = 0x40;
    } else {
        func_800532CC();
        D5B498_8006C228 = 0x80;
    }

    CdControlF(CdlPause, NULL);
}

INCLUDE_ASM("main/nonmatchings/12D18", func_8002576C);

INCLUDE_ASM("main/nonmatchings/12D18", func_800257A4);

INCLUDE_ASM("main/nonmatchings/12D18", func_800257B0);

INCLUDE_ASM("main/nonmatchings/12D18", func_80025850);

INCLUDE_ASM("main/nonmatchings/12D18", func_80025898);
