#include "common.h"

#include <psyq/libapi.h>
#include <psyq/libcd.h>
#include <psyq/libetc.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "main/unknown_syms.h"
#include "main/mem.h"
#include "main/fs.h"
#include "main/display.h"
#include "main/game.h"

void F12D18_80022518(void)
{
    D5B498_8006AC9A       = 0;
    CdCmd_Queue.field_224 = 0;
}

void F12D18_8002252C(u8* arg0, s16 arg1)
{
    CdCmd_Queue.field_224   = 1;
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
            if (CdCmd_IsSlotEmpty(D5B498_8006AC98)) {
                D5B498_8006AC9A = 2;
                D5B498_8006AC9C = 0;
            }
            break;
        case 2:
            func_8002226C(D5B498_8006ACB0, D5B498_8006ACAC);
            break;
    }
}

s32 Fs_LoadFile(u8* req, s32 mode, s32 a2, s32 a3)
{
    u8  modeU8;
    s32 sector;
    u32 i;
    u16 len;
    u32 fileId;

    sector                = 0;
    D5B498_8006ADF4       = 0;
    CdCmd_Queue.field_1FE = 0xFF;
    modeU8                = (u8)mode;

    if (req[3] == 0) {
        switch (req[2]) {
            case 0:
                if (req[1] != 0) {
                    goto setup_and_load;
                }
                if (req[0] == 0) {
                    Display_State.field_12a = 1;
                }
                if (req[0] == 1) {
                    Display_State.field_12a = 0;
                }
                sector = Fs_FileOffsetsCat0[req[0]] + Fs_StageCdfSectors[0];
                goto setup_and_load;

            case 1:
                fileId = (req[1] * 100) + req[0];
                len    = Fs_FileTableCat1Len;
                i      = 0;
                if ((u32)sector < (u32)len) {
                    do {
                        if (Fs_FileTableCat1[i].id == fileId) {
                            sector = Fs_FileTableCat1[i].offset + Fs_StageCdfSectors[0];
                            goto setup_and_load;
                        }
                        i++;
                    } while (i < (u32)len);
                }
                goto setup_and_load;

            case 2:
                fileId = (req[1] * 100) + req[0];
                len    = Fs_FileTableCat2Len;
                i      = 0;
                if ((u32)sector < (u32)len) {
                    do {
                        if (Fs_FileTableCat2[i].id == fileId) {
                            sector = Fs_FileTableCat2[i].offset + Fs_StageCdfSectors[0];
                            goto setup_and_load;
                        }
                        i++;
                    } while (i < (u32)len);
                }
                goto setup_and_load;

            case 3:
                fileId = (req[1] * 100) + req[0];
                len    = Fs_FileTableCat3Len;
                i      = 0;
                if ((u32)sector < (u32)len) {
                    do {
                        if (Fs_FileTableCat3[i].id == fileId) {
                            sector = Fs_FileTableCat3[i].offset + Fs_StageCdfSectors[0];
                            goto setup_and_load;
                        }
                        i++;
                    } while (i < (u32)len);
                }
                goto setup_and_load;

            case 4:
                Fs_CdOpStatus = 0xFF;
                if (req[1] == 1) {
                    fileId = req[1] * 100 + req[0];
                    len    = Fs_FileTableCat4Len;
                    i      = 0;
                    if ((u32)sector < (u32)len) {
                        do {
                            if (Fs_FileTableCat4[i].id == fileId) {
                                sector = Fs_FileTableCat4[i].offset + Fs_StageCdfSectors[0];
                                goto after4;
                            }
                            i++;
                        } while (i < (u32)len);
                    }
                after4:
                    if (sector == 0) {
                        return 0;
                    }
                    D5B498_8006ACC8 = 1;
                    Fs_ChunkMode    = 0;
                    Fs_ReadSector(sector);
                }
                goto end_return;

            case 5:
                sector = Fs_FileOffsetsCat5[req[0]] + Fs_StageCdfSectors[0];
                goto setup_and_load;

            case 0x5A:
                sector = Fs_FileOffsetsCat90[req[0]] + Fs_StageCdfSectors[0];
                goto setup_and_load;

            default:
                D5B498_8006ADF4 = req[2] / 10;
                fileId          = (req[2] * 10000) + (req[1] * 100) + req[0];
                if ((D5B498_8006ADF4 != 0) && ((len = Fs_FileTableLen) != 0)) {
                    i = 0;
                    do {
                        if (Fs_FileTable[i].id == fileId) {
                            sector = Fs_FileTable[i].offset + Fs_StageCdfSectors[0];
                            if (req[2] == 8) {
                                if ((u32)(req[1] - 1) < 3U) {
                                    Game_Session->field_4E = 1;
                                }
                            }
                            goto setup_and_load;
                        }
                        i++;
                    } while (i < (u32)len);
                }
                goto setup_and_load;
        }
    } else {
        sector = D_8006C158[req[0]] + Fs_StageCdfSectors[req[3]];
    }

setup_and_load:
    D5B498_8006C234 = a3;
    D5B498_8006C233 = a2;
    D5B498_8006ACC8 = 0;
    if (sector != 0) {
        switch (modeU8) {
            case 0:
                Fs_ChunkMode = 0;
                break;
            case 1:
                Fs_SeekToPos(sector);
                goto end_return;
            case 2:
                Snd_InitFromStage(Game_Session->field_7, Game_Session->field_6);
                Fs_ChunkMode = 1;
                break;
            case 3:
                Fs_ChunkMode = 2;
                break;
            case 4:
                Fs_ChunkMode = 3;
                break;
            case 5:
                Fs_ChunkMode = 4;
                break;
            case 6:
                Fs_ChunkMode = 5;
                break;
            default:
                Fs_ChunkMode = 0;
                break;
        }
        Fs_ReadSector(sector);
    }
end_return:
    return sector & 0xFFFF;
}

void Fs_CdReadyCb(u8 status, u8* result)
{
    CdlLOC currLoc[3];
    s32    currPos;
    u8     ret;

    if (status == CdlDiskError) {
        goto on_error;
    }

    Fs_VBlank = VSync(-1);
    CdGetSector(currLoc, 3);
    Fs_CurrSector = currPos = CdPosToInt(currLoc);

    if (currPos != Fs_ReqSector) {
        if ((Fs_Streaming != 0) && (Fs_LoadPhase != 6)) {
            F12D18_800256F4(FS_ERROR_HARD);
            goto end;
        }
        goto on_error;
    }

    Fs_ReqSector = currPos + 1;
    if (Fs_Streaming == 0) {
        ret = Fs_ProcessChunkHeader();
    } else {
        ret = Fs_ProcessChunkData();
    }
    goto check_ret;

on_error:
    F12D18_800256F4(FS_ERROR_SOFT);
    goto end;

check_ret:
    if (ret != 0) {
        CdControlF(CdlPause, NULL);
        CdReadyCallback(NULL);
        D5B498_8006C234 = 0;
        D5B498_8006C233 = 0;
        D5B498_8006ADF4 = 0;
        Fs_ChunkMode    = 0;
        Fs_CdOpStatus   = FS_CD_STATUS_IDLE;
    }
end:
    return;
}

INCLUDE_ASM("main/nonmatchings/fs", Fs_ProcessChunkHeader);

INCLUDE_ASM("main/nonmatchings/fs", Fs_ProcessChunkData);

void Fs_SelectStage(s32 stageIdx)
{
    CdlLOC loc[2];
    s32    sector;
    u8*    dest;

    Fs_ChunkMode    = 0;
    D5B498_8006ADF4 = 0;
    sector          = Fs_StageCdfSectors[(u8)stageIdx];
    dest            = (u8*)&Fs_CdSector;

    if (CdSync(1, NULL) == CdlDiskError) {
        Fs_WaitDiskReset(1);
    }

    Fs_CdOpStatus     = 0;
    Fs_ChunkEndFlag   = -1;
    Fs_LoadPhase      = 0;
    Fs_ReqSector      = sector;
    Fs_ChunkEndSector = sector;
    Fs_ChunkWritePtr  = dest;
    Fs_VBlank         = VSync(-1);

    if (Fs_SeekSector == sector) {
        CdControlF(CdlReadN, NULL);
        CdReadyCallback(F12D18_8002563C);
        Fs_SeekSector = 0;
    } else {
        CdIntToPos(sector, loc);
        CdControlF(CdlReadN, &loc[0].minute);
        CdSyncCallback(F12D18_8002563C);
        Fs_SeekSector = 0;
    }

    Fs_VBlank = VSync(-1);
}

void Fs_PrepareFolderLoad(s32 arg0, s32 arg1, s32 arg2)
{
    CdlLOC loc[2];
    s32    i;
    s32    folderId;
    s32    sector;
    u8*    dest;

    D_8006ADE8.field_0 = 0;
    D_8006ADE8.field_2 = 0;
    D_8006ADE8.field_4 = 0;
    D_8006ADE8.field_8 = 0;
    Fs_ChunkMode       = 0;
    D5B498_8006ADF4    = 0;

    for (i = 0; (u16)i < 0x32; i++) {
        D_8006C338[i & 0xFFFF].field_0 = 0;
    }

    for (i = 0; (u16)i < 0xF; i++) {
        Stream_Slots[i & 0xFFFF].field_4 = 0;
    }

    i = 0;
    asm("");
    D_8006ADE2 = 0;
    folderId   = ((u8)arg1 * 100) + (u8)arg2;

    for (; (u16)i < Fs_FolderTableLen; i++) {
        if (folderId == Fs_FolderTable[i & 0xFFFF].id) {
            break;
        }
    }

    Fs_VBlank = VSync(-1);

    sector = Fs_FolderTable[i & 0xFFFF].offset + Fs_StageCdfSectors[(u8)arg0];
    dest   = (u8*)&Fs_CdSector;

    if (CdSync(1, NULL) == CdlDiskError) {
        Fs_WaitDiskReset(1);
    }

    Fs_CdOpStatus     = 0;
    Fs_ChunkEndFlag   = -1;
    Fs_LoadPhase      = 0;
    Fs_ReqSector      = sector;
    Fs_ChunkEndSector = sector;
    Fs_ChunkWritePtr  = dest;
    Fs_VBlank         = VSync(-1);

    if (Fs_SeekSector == sector) {
        CdControlF(CdlReadN, NULL);
        CdReadyCallback(F12D18_8002563C);
        Fs_SeekSector = 0;
    } else {
        CdIntToPos(sector, loc);
        CdControlF(CdlReadN, &loc[0].minute);
        CdSyncCallback(F12D18_8002563C);
        Fs_SeekSector = 0;
    }
}

void func_8002397C(s32 arg0, s32 arg1, s32 arg2)
{
    s32          i;
    s32          j;
    s32          folderId;
    FsCdfFile*   files;
    s32*         table;
    FsCdfFile*   file;
    s32          offset;
    FsCdfFolder* folder;
    FsCdfStream* streams;
    FsCdfStream* stream;
    StreamSlot*  destBase;
    s32          k;
    u8*          src;
    u8*          dst;

    i        = 0;
    folderId = ((u8)arg1 * 100) + (u8)arg2;
    for (; (u16)i < Fs_FolderTableLen; i++) {
        if (folderId == Fs_FolderTable[i & 0xFFFF].id) {
            break;
        }
    }

    files = (FsCdfFile*)&Fs_CdSector;
    j     = 0;
    table = D_8006C158;
    {
        FsCdfFolder* sp = Fs_FolderTable;
        folder          = sp + (i & 0xFFFF);
    }
loop_files:
    file   = (FsCdfFile*)(((j & 0xFFFF) << 3) + (s32)files);
    offset = file->offset;
    j     += 1;
    if (offset != 0) {
        table[file->id] = offset + folder->offset;
        goto loop_files;
    }

    i        = 0;
    folderId = ((u8)arg1 * 100) + 1;
    streams  = (FsCdfStream*)(Fs_CdSector.bytes + 0x514);
    for (; (u16)i < Fs_FolderTableLen; i++) {
        if (folderId == Fs_FolderTable[i & 0xFFFF].id) {
            break;
        }
    }

    j = 0;
    {
        FsCdfFolder* sp = Fs_FolderTable;
        files           = (FsCdfFile*)(sp + (i & 0xFFFF));
    }
    {
        s32* sp = Fs_StageCdfSectors;
        table   = sp + (u8)arg0;
    }
    destBase = Stream_Slots;
loop_streams:
    stream = (FsCdfStream*)(((j & 0xFFFF) * 0x28) + (s32)streams);
    if (*(s32*)&stream->data.movie.field_c != 0) {
        src = (u8*)stream;
        asm("");
        dst             = (u8*)(((j & 0xFFFF) * 0x28) + (s32)destBase);
        stream->offset += files->offset + *table;
        for (k = 0; (u16)k < 0x28; k++) {
            dst[k & 0xFFFF] = src[k & 0xFFFF];
        }
        j += 1;
        goto loop_streams;
    }
}

#ifndef NON_MATCHING
INCLUDE_ASM("main/nonmatchings/fs", Fs_InitStage0TablesCb);
#else
void Fs_InitStage0TablesCb(u8 status, u8* result)
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
    Fs_VBlank = VSync(-1);
    CdGetSector(currLoc, 3);

    Fs_CurrSector = currPos = CdPosToInt(currLoc);
    if (currPos == Fs_ReqSector) {
        goto sector_start;
    } else {
        goto on_error;
    }

table_end:
    CdReadyCallback(NULL);
    Fs_CdOpStatus = -1;
    CdControlF(CdlPause, NULL);
    return;

sector_start:
    // Read the sector data.
    Fs_ReqSector += 1;
    CdGetSector(Fs_CdSector.words, FS_SECTOR_WORD_SIZE);

    headerOffset = 0;

    while (true) {
        sectorBuffer = &Fs_CdSector;
        streamTable  = Fs_Streams;
        fileSect0    = Fs_FileOffsetsCat0;
        fileSect5    = Fs_FileOffsetsCat5;
        fileSect90   = Fs_FileOffsetsCat90;

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
            streamTable[(u16)streamIdx++].offset += Fs_StageCdfSectors[0];
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
                    i = Fs_FileTableCat1Len;
                    Fs_FileTableCat1Len++;

                    Fs_FileTableCat1[i].id     = fileId - 10000;
                    Fs_FileTableCat1[i].offset = ((FsCdfFile*)entry)->offset;
                    isValidCategory            = true;
                    break;

                case 2:
                    i = Fs_FileTableCat2Len;
                    Fs_FileTableCat2Len++;

                    Fs_FileTableCat2[i].id     = fileId - fileCategory * 10000;
                    Fs_FileTableCat2[i].offset = ((FsCdfFile*)entry)->offset;
                    isValidCategory            = true;
                    break;

                case 3:
                    i = Fs_FileTableCat3Len;
                    Fs_FileTableCat3Len++;

                    Fs_FileTableCat3[i].id     = fileId - 30000;
                    Fs_FileTableCat3[i].offset = ((FsCdfFile*)entry)->offset;
                    isValidCategory            = true;
                    break;

                case 4: {
                    i = Fs_FileTableCat4Len;
                    Fs_FileTableCat4Len++;

                    Fs_FileTableCat4[i].id     = fileId - fileCategory * 10000;
                    Fs_FileTableCat4[i].offset = ((FsCdfFile*)entry)->offset;
                    isValidCategory            = true;
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
                    i = Fs_FileTableLen;
                    Fs_FileTableLen++;

                    Fs_FileTable[i].id     = fileId;
                    Fs_FileTable[i].offset = ((FsCdfFile*)entry)->offset;
                }
            }

            headerOffset += sizeof(FsCdfFile) / sizeof(u32);
        }
    }

on_error:
    F12D18_800256F4(FS_ERROR_SOFT);
}
#endif

INCLUDE_ASM("main/nonmatchings/fs", Fs_ScanIsoDirectory);

s32 Fs_LoadImageChunk(FsImageChunk* arg0, u8 arg1)
{
    register u_long* ot asm("s0");
    register s32     retry asm("s1");
    s32              none;
    u8               yAdj;
    FsImageChunk*    img;
    RECT*            rect;
    u_long*          z;
    u32              inRange;

    if (ResetRCnt(RCntCNT2) == 0) {
        return 0xFF;
    }

    none  = -1;
    retry = arg1;
    do {
        ot = BreakDraw();
        if ((s32)ot != none) {
            break;
        }
        if (GetRCnt(RCntCNT2) >= 0x6E40) {
            if (retry == 0) {
                Fs_ContinueDrawing((u_long*)-1);
                return 0x7F;
            }
        }
    } while (1);

    D_8006C4C8[D5B498_8006ADF4] = 0;
    Fs_ImageRect.x              = arg0->x;

    inRange = (u32)(arg0->y - 0xF5) < 0xBU;
    img     = arg0;
    if (inRange) {
        yAdj = D5B498_8006C234;
    } else {
        yAdj = 0;
    }

    if (Fs_ChunkMode == 2) {
        Fs_ImageRect.y = (s8)yAdj + (img->y + 1);
    } else {
        Fs_ImageRect.y = img->y + (s8)yAdj;
    }

    D5B498_8006C22C  = (u8*)(arg0 + 1);
    rect             = &Fs_ImageRect;
    rect->w          = img->w;
    Fs_ChunkWritePtr = (u8*)D5B498_8006D870;
    rect->h          = img->h;
    func_80010398();

    if (D5B498_8006D748 == 0xFFFF) {
        Fs_ContinueDrawing(ot);
        return 0x7F;
    }

    LoadImage2(rect, D5B498_8006D870);

    retry = arg1;
    z     = NULL;
    do {
    } while (IsIdleGPU(-1) != 0);

    if (GetRCnt(RCntCNT2) >= 0x6E40) {
        if (retry == 0) {
            ContinueDraw(z, ot);
            return 0x7F;
        }
        // Force a0 re-materialization of `ot` on the retry path so the
        // delay slot of `bnez s1` can hold `move a0, zero` for ContinueDraw
        // while the taken path restores a0 before Fs_ContinueDrawing.
        asm volatile("" : : : "$4");
    }

    D_8006C4C8[D5B498_8006ADF4] = (u8)Fs_ImageRect.h;
    Fs_ContinueDrawing(ot);
    return 0;
}

void Fs_CopyWorkEntries(FsWorkEntry* arg0)
{
    register FsWorkEntry* base asm("t0");
    register s32          term asm("t1");
    register u32          ace_hi asm("a3");
    FsWorkEntry*          src;
    FsWorkEntry*          dst;
    u32*                  mid;

    src  = arg0;
    term = 0xFFFF;
    /* Keep %hi(D5B498_8006ACE8) in $a3 across the copy loop so later
       field_0 loads can use lhu %lo(a3) (GCC will not CSE this itself). */
    __asm__(
        "lui %0, %%hi(D5B498_8006ACE8)\n\t"
        "addiu %1, %0, %%lo(D5B498_8006ACE8)"
        : "=&r"(ace_hi), "=r"(base));
    dst = base;
    mid = &arg0->field_4;

loop:
    dst->field_0 = src->field_0;
    dst->field_2 = ((u16*)mid)[-1];
    dst->field_4 = *mid;
    if (dst->field_0 != term) {
        mid += 2;
        src += 1;
        dst += 1;
        goto loop;
    }

    if ((base->field_2 >= 0x100U) || (Fs_ChunkMode == 2)) {
        s32  t;
        s32  c;
        s16* px;
        px = &Fs_ImageRect.x;
        __asm__("lhu %0, %%lo(D5B498_8006ACE8)(%1)" : "=r"(t) : "r"(ace_hi));
        c   = (s8)D5B498_8006C233 * 64;
        *px = t + c;
    } else {
        register s32  t asm("v1");
        register s16* px asm("v0");
        px = &Fs_ImageRect.x;
        __asm__("lhu %0, %%lo(D5B498_8006ACE8)(%1)" : "=r"(t) : "r"(ace_hi));
        *px = t;
    }

    if (Fs_ChunkMode == 2) {
        Fs_ImageRect.y = D5B498_8006ACE8[0].field_2 + 0x80;
    } else {
        Fs_ImageRect.y = D5B498_8006ACE8[0].field_2;
    }

    Fs_ImageRect.w  = 0x40;
    Fs_ImageRect.h  = 0x20;
    D5B498_8006ACD4 = 0x100;

    D5B498_8006C22C = (u8*)arg0 + D5B498_8006ACE8[0].field_4;

    if (D5B498_8006ACE8[1].field_0 == 0xFFFF) {
        if (D5B498_8006ACE8[1].field_2 == D5B498_8006ACE8[1].field_0) {
            D5B498_8006ACD4 = 0x40;
        } else if (D5B498_8006ACE8[1].field_2 & 0x8000) {
            D5B498_8006ACD4 = D5B498_8006ACE8[1].field_2 & 0x7FFF;
        }
    }

    D5B498_8006ADE1                  = 1;
    D5B498_8006ADE0                  = 1;
    D5B498_8006D4E0[D5B498_8006ADF4] = 0;
}

INCLUDE_ASM("main/nonmatchings/fs", Fs_LoadImageStrip);

void Fs_ClearDiskError(void)
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

    CdIntToPos(Fs_ReqSector, loc);
    CdControlF(CdlReadN, &loc[0].minute);
    if (Fs_CdOpStatus == 0x40) {
        CdSyncCallback(F12D18_80025580);
        Fs_VBlank      = VSync(-1);
        Fs_CdOpStatus += 1;
    }
}

u8 Fs_WaitDiskSwap(void)
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

void Fs_ReadSectorEx(s32 sector, s32 arg1, u8* arg2, u8 arg3)
{
    CdlLOC loc[2];

    if (CdSync(1, NULL) == CdlDiskError) {
        Fs_WaitDiskReset(true);
    }

    Fs_CdOpStatus     = 0;
    Fs_ChunkEndFlag   = -1;
    Fs_LoadPhase      = arg3;
    Fs_ReqSector      = sector;
    Fs_ChunkEndSector = arg1;
    Fs_ChunkWritePtr  = arg2;
    Fs_VBlank         = VSync(-1);
    if (Fs_SeekSector == sector) {
        CdControlF(CdlReadN, NULL);
        CdReadyCallback(F12D18_8002563C);
        Fs_SeekSector = 0;
    } else {
        CdIntToPos(sector, loc);
        CdControlF(CdlReadN, &loc[0].minute);
        CdSyncCallback(F12D18_8002563C);
        Fs_SeekSector = 0;
    }
}

void Fs_ReadSector(s32 sector)
{
    CdlLOC loc[2];

    if (CdSync(1, NULL) == CdlDiskError) {
        Fs_WaitDiskReset(true);
    }

    Fs_CdOpStatus   = 0;
    Fs_ChunkEndFlag = -1;
    Fs_ReqSector    = sector;
    Fs_VBlank       = VSync(-1);
    if (Fs_SeekSector == sector) {
        Fs_SeekSector = 0;
        Fs_Streaming  = false;
        CdControlF(CdlReadN, NULL);
        CdReadyCallback(Fs_CdReadyCb);
    } else {
        Fs_SeekSector = 0;
        CdIntToPos(sector, loc);
        CdControlF(CdlReadN, &loc[0].minute);
        CdSyncCallback(F12D18_80025580);
    }
}

void Fs_WaitDiskReset(s8 withSectHdr)
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

void Fs_SeekToPos(s32 sector)
{
    CdlLOC loc[2];

    Fs_CdOpStatus = 0;
    if (CdSync(1, NULL) == CdlDiskError) {
        Fs_WaitDiskReset(true);
    }

    Fs_SeekSector = sector;
    CdIntToPos(sector, loc);
    CdControlF(CdlSeekL, (u8*)loc);
    CdSyncCallback(Fs_SeekToPosCb);
    Fs_VBlank = VSync(-1);
}

void Fs_InitFolderTable(s32 arg0)
{
    u32                   offset;
    FsCdfFolderListEntry* entry;

    Fs_FolderTableLen = 0;
    entry             = Fs_CdSector.folderList.entries;

    // The cdf file starts with a header section. Therefore, the offset to
    // the start of the first folder is always one. Following folders are
    // located immediately after the previous ones.
    offset = 1;
    while (true) {
        if (entry->size == FS_CDF_FOLDER_CANARY) {
            return;
        }

        // Save the folder info into the table.
        Fs_FolderTable[Fs_FolderTableLen].id     = entry->id;
        Fs_FolderTable[Fs_FolderTableLen].offset = offset;
        Fs_FolderTableLen                       += 1;

        // Go to the next entry.
        offset += entry->size;
        entry  += 1;
    }
}

void Fs_InitStage0Tables(void)
{
    CdlLOC headerPos;

    // Reset all tables.
    Fs_CdOpStatus       = 0;
    Fs_FileTableLen     = 0;
    Fs_FileTableCat2Len = 0;
    Fs_FileTableCat4Len = 0;
    Fs_FileTableCat1Len = 0;
    Fs_FileTableCat3Len = 0;

    // Read the stage header.
    Fs_ReqSector = Fs_Stage0HedSector;
    CdIntToPos(Fs_Stage0HedSector, &headerPos);
    CdControlF(CdlReadN, &headerPos.minute);
    CdReadyCallback(Fs_InitStage0TablesCb);
    Fs_VBlank = VSync(-1);
}

void F12D18_80025580(u8 status, u8* result)
{
    if (status != CdlDiskError) {
        if (Fs_CdOpStatus == 0x41) {
            Fs_CdOpStatus = 0;
            Fs_Streaming  = true;
            CdReadyCallback(Fs_CdReadyCb);
        } else if (D5B498_8006ACC8 == false) {
            Fs_Streaming = false;
            CdReadyCallback(Fs_CdReadyCb);
        } else {
            SndLoad_FromSectorMode8(&Fs_CdSector);
            CdReadyCallback(Fs_StreamReadyCb);
        }

        Fs_VBlank = VSync(-1);
        CdSyncCallback(NULL);
        Fs_CdErrorCount = 0;
    } else {
        F12D18_800256F4(FS_ERROR_SOFT);
    }
}

void F12D18_8002563C(u8 status, u8* result)
{
    if (status != CdlDiskError) {
        Fs_VBlank       = VSync(-1);
        Fs_CdErrorCount = 0;
        Fs_Streaming    = 1;
        CdReadyCallback(Fs_CdReadyCb);
        CdSyncCallback(NULL);
    } else {
        F12D18_800256F4(FS_ERROR_SOFT);
    }
}

void Fs_SeekToPosCb(u8 status, u8* result)
{
    if (status == CdlComplete) {
        Fs_CdOpStatus = FS_CD_STATUS_IDLE;
        CdSyncCallback(NULL);
        Fs_CdErrorCount = 0;
    } else {
        F12D18_800256F4(FS_ERROR_SOFT);
    }
}

void F12D18_800256F4(u8 arg0)
{
    Fs_CdErrorCount += 1;
    CdReadyCallback(NULL);
    CdSyncCallback(NULL);

    if (arg0 == FS_ERROR_HARD) {
        Fs_CdOpStatus = 0x40;
    } else {
        SndLoad_Teardown();
        Fs_CdOpStatus = 0x80;
    }

    CdControlF(CdlPause, NULL);
}

void Fs_ContinueDrawing(u_long* ot)
{
    // Wait until the gpu is idling and then continue drawing.
    while (IsIdleGPU(-1) != 0) {
    }
    ContinueDraw(NULL, ot);
}

u8* Fs_GetChunkPayload(void)
{
    return &Fs_CdSector.bytes[0x10];
}

void F12D18_800257B0(void)
{
    u8 ctrlResult[8];

    if (Fs_CdOpStatus != 0 && Fs_CdOpStatus != 0x41) {
        return;
    }

    if (VSync(-1) <= Fs_VBlank + 0xb4) {
        return;
    }

    Fs_VBlank     = VSync(-1);
    Fs_CdOpStatus = 0x80;
    CdFlush();
    SndLoad_Teardown();
    CdReadyCallback(NULL);
    CdSyncCallback(NULL);
    CdControlB(CdlPause, NULL, ctrlResult);
}

void Fs_StopCd(void)
{
    u8 ctrlParam[9];

    ctrlParam[0] = 0;
    ctrlParam[8] = 0;
    CdControlB(CdlSetmode, ctrlParam, NULL);

    VSync(3);
    CdControlB(CdlStop, NULL, NULL);
}

s32 func_80025898(void)
{
    u8 stage;

    stage = Game_Session->field_7;
    if (Fs_StageCdfSectors[stage] == 0) {
        if (stage == 1 || stage == 2) {
            return 1;
        }
        if (stage == 4 || stage == 5) {
            return 2;
        }
    }
    return 0;
}
