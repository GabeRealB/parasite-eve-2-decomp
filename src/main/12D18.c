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

#ifndef NON_MATCHING
INCLUDE_ASM("main/nonmatchings/12D18", F12D18_InitStage0TablesCb);
#else
void F12D18_InitStage0TablesCb(u8 status, u8* result)
{
    CdlLOC currLoc[3];
    s32    currPos;
    u32    headerOffset;
    u32    streamIdx;

    u32         fileId;
    u32         fileCategory;
    register u8 isValidCategory asm("t1") = false;

    u32          i;
    u32          tableLen;
    u32*         entry;
    u8*          entryBytes asm("a2");
    register u32 entryValue asm("a3");

    u8* streamCpyPos;

    FsSector*    sectorBuffer;
    FsCdfStream* streamTable;
    u32*         fileSect90;
    u16*         fileSect5;
    u16*         fileSect0;

    streamIdx = 0;
    if (status == CdlDiskError) {
        goto on_error;
    }

    // The first 3 words contain the sector header.
    // Make sure that we seeked to the correct location.
    D5B498_CurrVBlank = VSync(-1);
    CdGetSector(currLoc, 3);

    D5B498_CurrCdSector = currPos = CdPosToInt(currLoc);
    if (currPos == D5B498_ReqCdSector) {
        goto sector_start;
    } else {
        goto on_error;
    }

table_end:
    CdReadyCallback(NULL);
    D5B498_8006C228 = -1;
    CdControlF(CdlPause, NULL);
    return;

sector_start:
    // Read the sector data.
    D5B498_ReqCdSector += 1;
    CdGetSector(D5B498_CdSectorBuffer.words, FS_SECTOR_WORD_SIZE);

    headerOffset = 0;

    while (true) {
        sectorBuffer = &D5B498_CdSectorBuffer;
        streamTable  = D5B498_Streams;
        fileSect0    = D5B498_Stage0FileSect0;
        fileSect5    = D5B498_Stage0FileSect5;
        fileSect90   = D5B498_Stage0FileSect90;

        if ((u16)headerOffset >= FS_SECTOR_WORD_SIZE)
            return;

        entry      = &sectorBuffer->words[headerOffset];
        entryValue = *entry;
        if (entryValue == FS_CDF_STAGE0_CANARY) {
            goto table_end;
        }

        if ((s32)entryValue < 0) {
            *entry    &= 0x7fffffff;
            entryBytes = (u8*)entry;

            // Copy the stream header into the stream table.
            streamCpyPos = (u8*)&streamTable[(u16)streamIdx];
            for (i = 0; (u16)i < sizeof(FsCdfStream); i++) {
                streamCpyPos[(u16)i] = entryBytes[(u16)i];
            }

            // Move to the next entry and adjust the offset to be the absolute
            // offset on the CD rom.
            streamTable[(u16)streamIdx++].offset += D5B498_StageSectors[0];
            headerOffset                         += sizeof(FsCdfStream) / sizeof(u32);
        } else {
            fileId       = entryValue;
            fileCategory = fileId / 10000;

            isValidCategory = false;
            switch (fileCategory) {
                case 0:
                    i = 0;
                    while (true) {
                        fileSect0[(u16)i] = ((FsCdfFile*)&sectorBuffer->words[(u16)headerOffset])->offset;
                        i++;
                        if ((u16)i >= 0x2D)
                            break;
                        headerOffset += 2;
                    }
                    isValidCategory = true;
                    break;

                case 1:
                    i = D5B498_Stage0FileTable1Len;
                    D5B498_Stage0FileTable1Len++;

                    D5B498_Stage0FileTable1[i].id     = fileId - 10000;
                    D5B498_Stage0FileTable1[i].offset = ((FsCdfFile*)entry)->offset;
                    isValidCategory                   = true;
                    break;

                case 2:
                    i = D5B498_Stage0FileTable2Len;
                    D5B498_Stage0FileTable2Len++;

                    D5B498_Stage0FileTable2[i].id     = fileId - fileCategory * 10000;
                    D5B498_Stage0FileTable2[i].offset = ((FsCdfFile*)entry)->offset;
                    isValidCategory                   = true;
                    break;

                case 3:
                    i = D5B498_Stage0FileTable3Len;
                    D5B498_Stage0FileTable3Len++;

                    D5B498_Stage0FileTable3[i].id     = fileId - 30000;
                    D5B498_Stage0FileTable3[i].offset = ((FsCdfFile*)entry)->offset;
                    isValidCategory                   = true;
                    break;

                case 4: {
                    i = D5B498_Stage0FileTable4Len;
                    D5B498_Stage0FileTable4Len++;

                    D5B498_Stage0FileTable4[i].id     = fileId - fileCategory * 10000;
                    D5B498_Stage0FileTable4[i].offset = ((FsCdfFile*)entry)->offset;
                    isValidCategory                   = true;
                    break;
                }

                case 5:
                    fileSect5[*entry % 100] = ((FsCdfFile*)entry)->offset;
                    isValidCategory         = true;
                    break;

                case 90:
                    fileSect90[*entry % 100] = ((FsCdfFile*)entry)->offset;
                    isValidCategory          = true;
                    break;
            }

            if (!isValidCategory) {
                if (sectorBuffer->words[(u16)headerOffset] / 100000 != 0) {
                    i = D5B498_Stage0FileTableLen;
                    D5B498_Stage0FileTableLen++;

                    D5B498_Stage0FileTable[i].id     = fileId;
                    D5B498_Stage0FileTable[i].offset = ((FsCdfFile*)entry)->offset;
                }
            }

            headerOffset += sizeof(FsCdfFile) / sizeof(u32);
        }
    }

on_error:
    F12D18_800256F4(C12D18_800256F4_ARG_0);
}
#endif

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
