#include "common.h"

#include <psyq/libapi.h>
#include <psyq/libcd.h>
#include <psyq/libetc.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "main/unknown_syms.h"
#include "main/mem.h"
#include "main/fs.h"

void F12D18_80022518(void)
{
    D5B498_8006AC9A      = 0;
    D_80068FA0.field_224 = 0;
}

void F12D18_8002252C(u8* arg0, s16 arg1)
{
    D_80068FA0.field_224    = 1;
    D5B498_8006ACB8.field_3 = arg0[3];
    D5B498_8006ACB8.field_2 = arg0[2];
    D5B498_8006ACC0         = arg1;

    Mem_Set(D4CB64_ImgBuffers, 0, sizeof(*D4CB64_ImgBuffers));
    func_800280F4(1);
    D5B498_8006AC9A = 0;
}

void F12D18_80022598(void)
{
    if (D5B498_8006AC9A == 0) {
        func_8002207C();
        D5B498_8006AC9A = 1;
    }
}

void F12D18_800225D4(void)
{
    switch (D5B498_8006AC9A) {
        case 0:
            break;
        case 1:
            if (func_8001D37C(D5B498_8006AC98)) {
                D5B498_8006AC9A = 2;
                D5B498_8006AC9C = 0;
            }
            break;
        case 2:
            func_8002226C(D5B498_8006ACB0, D5B498_8006ACAC);
            break;
    }
}

INCLUDE_ASM("main/nonmatchings/12D18", func_8002265C);

void func_80022BD0(u8 status, u8* result)
{
    CdlLOC currLoc[3];
    s32    currPos;
    u8     ret;

    if (status == CdlDiskError) {
        goto on_error;
    }

    D5B498_CurrVBlank   = VSync(-1);
    CdGetSector(currLoc, 3);
    D5B498_CurrCdSector = currPos = CdPosToInt(currLoc);

    if (currPos != D5B498_ReqCdSector) {
        if ((D5B498_8006C231 != 0) && (D5B498_8006C230 != 6)) {
            F12D18_800256F4(C12D18_800256F4_ARG_2);
            goto end;
        }
        goto on_error;
    }

    D5B498_ReqCdSector = currPos + 1;
    if (D5B498_8006C231 == 0) {
        ret = func_80022CF0();
    } else {
        ret = func_800231A8();
    }
    goto check_ret;

on_error:
    F12D18_800256F4(C12D18_800256F4_ARG_0);
    goto end;

check_ret:
    if (ret != 0) {
        CdControlF(CdlPause, NULL);
        CdReadyCallback(NULL);
        D5B498_8006C234 = 0;
        D5B498_8006C233 = 0;
        D5B498_8006ADF4 = 0;
        D5B498_8006C232 = 0;
        D5B498_8006C228 = D5B498_8006C228_FF;
    }
end:
    return;
}

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

    register u32 i asm("a0");
    u32*         entry;
    u8*          entryBytes;
    u32          entryValue;

    u8* streamCpyPos;

    FsSector*    sectorBuffer;
    u32*         words;
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

        entry      = &sectorBuffer->words[(u16)headerOffset];
        entryValue = *entry;
        fileId     = entryValue;
        if (fileId == FS_CDF_STAGE0_CANARY) {
            goto table_end;
        }

        if ((s32)fileId < 0) {
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
                words = sectorBuffer->words;
                if (words[(u16)headerOffset] / 100000 != 0) {
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

void F12D18_ClearDiskError(void)
{
    u8  done;
    s32 status;
    u8  ctrlParam[8];
    u8  ctrlResult[8];

    done = 0;
    do {
        // Poll the status of the current command.
        status = CdSync(1, NULL);
        switch (status) {
            case CdlNoIntr:
                break;
            case CdlComplete:
                done = 1;
                break;
            case CdlDiskError:
                // Wait for the command to finish and reset the operation mode.
                CdSync(0, NULL);
                ctrlParam[0] = 0;
                CdControlB(CdlSetmode, ctrlParam, NULL);
                VSync(3);

                // Wait until the CD shell is closed with a valid disk.
                do {
                    do {
                        CdControlB(CdlNop, NULL, ctrlResult);
                    } while (ctrlResult[0] & CdlStatShellOpen);
                } while (CdDiskReady(0) != CdlComplete || CdGetDiskType() != CdlCdromFormat);

                // Enable double speed and sector header.
                ctrlParam[0] = CdlModeSpeed | CdlModeSize1;
                CdControlB(CdlSetmode, ctrlParam, NULL);
                VSync(3);
        }
    } while (done == 0);
}

void F12D18_80024EC0(void)
{
    CdlLOC loc[2];
    u8     ctrlParam[8];
    u8     ctrlResult[8];
    u8*    ctrlParamPtr;

    if (CdSync(1, NULL) == CdlDiskError) {
        // Wait for the command to finish and reset the operation mode.
        CdSync(0, NULL);
        ctrlParam[0] = 0;
        ctrlParamPtr = ctrlParam;
        CdControlB(CdlSetmode, ctrlParamPtr, NULL);
        VSync(3);

        // Wait until the CD shell is closed with a valid disk.
        do {
            do {
                CdControlB(CdlNop, NULL, ctrlResult);
            } while ((ctrlResult[0] & CdlStatShellOpen) != 0);
        } while (CdDiskReady(0) != CdlComplete || CdGetDiskType() != CdlCdromFormat);

        // Enable double speed and sector header.
        ctrlParamPtr[0] = CdlModeSpeed | CdlModeSize1;
        CdControlB(CdlSetmode, ctrlParam, NULL);
        VSync(3);
    }

    CdIntToPos(D5B498_ReqCdSector, loc);
    CdControlF(CdlReadN, &loc[0].minute);
    if (D5B498_8006C228 == 0x40) {
        CdSyncCallback(F12D18_80025580);
        D5B498_CurrVBlank = VSync(-1);
        D5B498_8006C228  += 1;
    }
}

u8 F12D18_WaitDiskSwap(void)
{
    s32    status;
    u8     ctrlParam[8];
    u8     ctrlResult[8];
    CdlLOC loc[2];

    VSync(0);

    // Wait until the CD shell is opened.
    do {
        CdControlB(CdlNop, NULL, ctrlResult);
    } while ((ctrlResult[0] & CdlStatShellOpen) == 0);

    // Wait until the CD shell is closed.
    do {
        CdControlB(CdlNop, NULL, ctrlResult);
    } while ((ctrlResult[0] & CdlStatShellOpen) != 0);

    // Wait until the CD is spinning.
    while ((ctrlResult[0] & CdlStatStandby) == 0) {
        CdControlB(CdlNop, NULL, ctrlResult);
    }

    // Wait until the TOC is read.
    status = CdControlB(CdlGetTN, NULL, ctrlResult);
    while (ctrlResult[0] != CdlStatStandby || status == CdlNoIntr) {
        VSync(0x1e);
        status = CdControlB(CdlGetTN, NULL, ctrlResult);
    }

    // Enable double speed.
    ctrlParam[0] = CdlModeSpeed;
    CdControlB(CdlSetmode, ctrlParam, NULL);
    VSync(3);

    // Read the volume descriptor (sector 16).
    CdIntToPos(0x10, loc);
    CdControlB(CdlReadN, &loc[0].minute, ctrlResult);

    // If we encountered an error, we return -1.
    // Apparently, the developers performed additional checks, but they all
    // return the same value.
    if (CdSync(0, ctrlResult) == CdlDiskError) {
        register u8 tmp asm("v1");
        if ((ctrlResult[0] & CdlStatError) == 0 || (tmp = ctrlResult[1], (tmp & 0x40) == 0)) {
            return -1;
        }

        // This prevents the compiler from optimizing away the branch.
        asm("");
        return -1;
    }

    // Enable the sector header.
    CdControlB(CdlPause, NULL, ctrlResult);
    ctrlParam[0] = CdlModeSpeed | CdlModeSize1;
    CdControlB(CdlSetmode, ctrlParam, NULL);
    return 0;
}

void F12D18_ReadSector2(s32 sector, s32 arg1, u8* arg2, u8 arg3)
{
    CdlLOC loc[2];

    if (CdSync(1, NULL) == CdlDiskError) {
        F12D18_WaitDiskReset(true);
    }

    D5B498_8006C228    = 0;
    D5B498_CdfEndFlag  = -1;
    D5B498_8006C230    = arg3;
    D5B498_ReqCdSector = sector;
    D5B498_8006D854    = arg1;
    D5B498_8006D4D8    = arg2;
    D5B498_CurrVBlank  = VSync(-1);
    if (D5B498_SeekPos == sector) {
        CdControlF(CdlReadN, NULL);
        CdReadyCallback(F12D18_8002563C);
        D5B498_SeekPos = 0;
    } else {
        CdIntToPos(sector, loc);
        CdControlF(CdlReadN, &loc[0].minute);
        CdSyncCallback(F12D18_8002563C);
        D5B498_SeekPos = 0;
    }
}

void F12D18_ReadSector(s32 sector)
{
    CdlLOC loc[2];

    if (CdSync(1, NULL) == CdlDiskError) {
        F12D18_WaitDiskReset(true);
    }

    D5B498_8006C228    = 0;
    D5B498_CdfEndFlag  = -1;
    D5B498_ReqCdSector = sector;
    D5B498_CurrVBlank  = VSync(-1);
    if (D5B498_SeekPos == sector) {
        D5B498_SeekPos  = 0;
        D5B498_8006C231 = false;
        CdControlF(CdlReadN, NULL);
        CdReadyCallback(func_80022BD0);
    } else {
        D5B498_SeekPos = 0;
        CdIntToPos(sector, loc);
        CdControlF(CdlReadN, &loc[0].minute);
        CdSyncCallback(F12D18_80025580);
    }
}

void F12D18_WaitDiskReset(s8 withSectHdr)
{
    u8 ctrlParam[8];
    u8 ctrlResult[8];

    // Wait for the command to finish and reset the operation mode.
    CdSync(0, NULL);
    ctrlParam[0] = 0;
    CdControlB(CdlSetmode, ctrlParam, NULL);
    VSync(3);

    // Wait until the CD shell is opened and closed again with a valid disk.
    do {
        do {
            CdControlB(CdlNop, NULL, ctrlResult);
        } while ((ctrlResult[0] & CdlStatShellOpen) != 0);
    } while (CdDiskReady(0) != CdlComplete || CdGetDiskType() != CdlCdromFormat);

    // Enable double speed and optionally also the sector header.
    if (withSectHdr != 0) {
        ctrlParam[0] = CdlModeSpeed | CdlModeSize1;
    } else {
        ctrlParam[0] = CdlModeSpeed;
    }
    CdControlB(CdlSetmode, ctrlParam, NULL);
    VSync(3);
}

void F12D18_SeekToPos(s32 sector)
{
    CdlLOC loc[2];

    D5B498_8006C228 = 0;
    if (CdSync(1, NULL) == CdlDiskError) {
        F12D18_WaitDiskReset(true);
    }

    D5B498_SeekPos = sector;
    CdIntToPos(sector, loc);
    CdControlF(CdlSeekL, (u8*)loc);
    CdSyncCallback(F12D18_SeekToPosCb);
    D5B498_CurrVBlank = VSync(-1);
}

void F12D18_InitFolderTable(void)
{
    u32                   offset;
    FsCdfFolderListEntry* entry;

    D5B498_FolderTableLen = 0;
    entry                 = D5B498_CdSectorBuffer.folderList.entries;

    // The cdf file starts with a header section. Therefore, the offset to
    // the start of the first folder is always one. Following folders are
    // located immediately after the previous ones.
    offset = 1;
    while (true) {
        if (entry->size == FS_CDF_FOLDER_CANARY) {
            return;
        }

        // Save the folder info into the table.
        D5B498_FolderTable[D5B498_FolderTableLen].id     = entry->id;
        D5B498_FolderTable[D5B498_FolderTableLen].offset = offset;
        D5B498_FolderTableLen                           += 1;

        // Go to the next entry.
        offset += entry->size;
        entry  += 1;
    }
}

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

void F12D18_80025580(u8 status, u8* result)
{
    if (status != CdlDiskError) {
        if (D5B498_8006C228 == 0x41) {
            D5B498_8006C228 = 0;
            D5B498_8006C231 = true;
            CdReadyCallback(func_80022BD0);
        } else if (D5B498_8006ACC8 == false) {
            D5B498_8006C231 = false;
            CdReadyCallback(func_80022BD0);
        } else {
            func_8005325C(&D5B498_CdSectorBuffer);
            CdReadyCallback(func_80025C94);
        }

        D5B498_CurrVBlank = VSync(-1);
        CdSyncCallback(NULL);
        D5B498_CdErrorCount = 0;
    } else {
        F12D18_800256F4(C12D18_800256F4_ARG_0);
    }
}

void F12D18_8002563C(u8 status, u8* result)
{
    if (status != CdlDiskError) {
        D5B498_CurrVBlank   = VSync(-1);
        D5B498_CdErrorCount = 0;
        D5B498_8006C231     = 1;
        CdReadyCallback(func_80022BD0);
        CdSyncCallback(NULL);
    } else {
        F12D18_800256F4(C12D18_800256F4_ARG_0);
    }
}

void F12D18_SeekToPosCb(u8 status, u8* result)
{
    if (status == CdlComplete) {
        D5B498_8006C228 = D5B498_8006C228_FF;
        CdSyncCallback(NULL);
        D5B498_CdErrorCount = 0;
    } else {
        F12D18_800256F4(C12D18_800256F4_ARG_0);
    }
}

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

void F12D18_ContinueDrawing(u_long* ot)
{
    // Wait until the gpu is idling and then continue drawing.
    while (IsIdleGPU(-1) != 0) {
    }
    ContinueDraw(NULL, ot);
}

u8* func_800257A4(void)
{
    return &D5B498_CdSectorBuffer.bytes[0x10];
}

void F12D18_800257B0(void)
{
    u8 ctrlResult[8];

    if (D5B498_8006C228 != 0 && D5B498_8006C228 != 0x41) {
        return;
    }

    if (VSync(-1) <= D5B498_CurrVBlank + 0xb4) {
        return;
    }

    D5B498_CurrVBlank = VSync(-1);
    D5B498_8006C228   = 0x80;
    CdFlush();
    func_800532CC();
    CdReadyCallback(NULL);
    CdSyncCallback(NULL);
    CdControlB(CdlPause, NULL, ctrlResult);
}

void F12D18_StopCd(void)
{
    u8 ctrlParam[9];

    ctrlParam[0] = 0;
    ctrlParam[8] = 0;
    CdControlB(CdlSetmode, ctrlParam, NULL);

    VSync(3);
    CdControlB(CdlStop, NULL, NULL);
}

INCLUDE_ASM("main/nonmatchings/12D18", func_80025898);
