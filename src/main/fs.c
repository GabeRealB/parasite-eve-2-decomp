#include "common.h"

#include <psyq/libapi.h>
#include <psyq/libcd.h>
#include <psyq/libetc.h>
#include <psyq/strings.h>

#include "main/display.h"
#include "main/fs.h"
#include "main/gamemain.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/stream.h"
#include "main/wipsys.h"

void Fs_ResetBootLoadState(void)
{
    Fs_BootLoadPhase      = 0;
    CdCmd_Queue.field_224 = 0;
}

void Fs_BeginBootLoad(u8* arg0, s16 arg1)
{
    CdCmd_Queue.field_224 = 1;
    Fs_LoadParams.field_3 = arg0[3];
    Fs_LoadParams.field_2 = arg0[2];
    D5B498_8006ACC0       = arg1;

    Mem_Set(Fs_ImgBuffers, 0, sizeof(*Fs_ImgBuffers));
    GameMain_SetFrameTiming(1);
    Fs_BootLoadPhase = 0;
}

void Fs_EnsureBootLoadStarted(void)
{
    if (Fs_BootLoadPhase == 0) {
        Fs_SetupBootLoad();
        Fs_BootLoadPhase = 1;
    }
}

void Fs_StepBootImage(void)
{
    switch (Fs_BootLoadPhase) {
        case 0:
            break;
        case 1:
            if (CdCmd_IsSlotEmpty(Fs_BootLoadSlot)) {
                Fs_BootLoadPhase = 2;
                D5B498_8006AC9C  = 0;
            }
            break;
        case 2:
            Fs_BootImageMachine(Fs_BootTimPrimary, Fs_BootTimSecondary);
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
            Fs_OnCdError(FS_ERROR_HARD);
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
    Fs_OnCdError(FS_ERROR_SOFT);
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

u8 Fs_ProcessChunkHeader(void)
{
    s32          s2;
    FsSector*    sec;
    register s32 endSect asm("a2");
    s32          type;
    s32          endFlag;
    s32          status;
    s32          tmp;
    s32*         sp;
    s32          f2;
    s32          d748;

    /*
     * Pure C prologue: sw s1 / lui s1 / sw s0 / addiu s0,s1,lo / move a0 /
     * sw ra / jal.  Absolute Fs_CdSector.chunk.type after setup keeps s1=%hi.
     */
    s2  = 0;
    sec = &Fs_CdSector;
    CdGetSector(sec->bytes, 0x200);

    /* Setup through endFlag; a1=%hi(D_8006C4D4) kept live (not clobbered). */
    {
        register s32 d4_hi asm("a1");
        __asm__ volatile(
            ".set\tnoreorder\n\t"
            "lui\t%1, %%hi(D_8006C4D4)\n\t"
            "lui\t$2, %%hi(Fs_ChunkWritePtr)\n\t"
            "lw\t$3, 8(%2)\n\t"
            "lui\t$4, %%hi(Fs_ChunkEndSector)\n\t"
            "sw\t%2, %%lo(D_8006C4D4)(%1)\n\t"
            "sw\t$3, %%lo(Fs_ChunkWritePtr)($2)\n\t"
            "lui\t$3, %%hi(Fs_ReqSector)\n\t"
            "lw\t$2, 4(%2)\n\t"
            "lw\t$7, %%lo(Fs_ReqSector)($3)\n\t"
            "addiu\t$2, $2, -1\n\t"
            "addu\t%0, $7, $2\n\t"
            "sw\t%0, %%lo(Fs_ChunkEndSector)($4)\n\t"
            "lbu\t$4, 1(%2)\n\t"
            "lui\t$2, %%hi(Fs_ChunkEndFlag)\n\t"
            "sb\t$4, %%lo(Fs_ChunkEndFlag)($2)\n\t"
            ".set\treorder"
            : "=&r"(endSect), "=&r"(d4_hi)
            : "r"(sec)
            : "v0", "a0", "a3", "memory");

        /* lhu field_2; lbu type via absolute chunk.type; addu; sw via a1 */
        f2 = sec->chunk.field_2;
        TOUCH_REG(f2);
        type = Fs_CdSector.chunk.type;
        __asm__ volatile(
            ".set\tnoreorder\n\t"
            "addu\t%0, %0, %2\n\t"
            "sw\t%0, %%lo(D_8006C4D4)(%1)\n\t"
            ".set\treorder"
            : "+r"(f2)
            : "r"(d4_hi), "r"(sec)
            : "memory");
    }

    switch (type) {
        case 0:
            if (Fs_ChunkWritePtr == 0) {
                goto ret0;
            }
            if (Fs_ChunkMode == 1) {
                goto phase_ff;
            }
            if (Fs_ChunkMode == 4) {
                goto phase_ff;
            }
            D5B498_8006EA1A = 0;
            D5B498_8006EBB0 = 0;
            D5B498_8006D858 = 1;
            D5B498_8006D850 = 0;
            D5B498_8006D748 = 0;
            Fs_ChunkReadPtr = Fs_CdSector.bytes + 0x10;
            Fs_DecompressChunk();
            d748 = D5B498_8006D748;
            __asm__ volatile(
                ".set\tnoreorder\n\t"
                "ori\t$2, $0, 0xFFFF\n\t"
                "beq\t%0, $2, %3\n\t"
                "addiu\t$17, $0, 1\n\t"
                "beqz\t%0, 1f\n\t"
                "lui\t$2, %%hi(Fs_ChunkEndFlag)\n\t"
                "lbu\t$3, %%lo(Fs_ChunkEndFlag)($2)\n\t"
                "j\t%1\n\t"
                "addiu\t$2, $0, 0xFF\n\t"
                "1:\n\t"
                "lui\t$2, %%hi(Fs_LoadPhase)\n\t"
                "sb\t$17, %%lo(Fs_LoadPhase)($2)\n\t"
                "lui\t$2, %%hi(Fs_Streaming)\n\t"
                "j\t%2\n\t"
                "sb\t$17, %%lo(Fs_Streaming)($2)\n\t"
                ".set\treorder"
                :
                : "r"(d748), "i"(&&check_end_ff), "i"(&&ret0), "i"(&&soft_error)
                : "v0", "s1", "memory");

        case 1: {
            s32 ff;
            Fs_CopyWorkEntries((FsWorkEntry*)(Fs_CdSector.bytes + 0x10));
            status = Fs_LoadImageStrip(0) & 0xFF;
            ff     = 0xFF;
            if (status == ff) {
                goto soft_error;
            }
            if (status == 0x7F) {
                goto soft_error;
            }
            {
                register s32 one asm("a0");
                s32          st;
                one = 1;
                st  = status;
                __asm__ volatile(
                    ".set\tnoreorder\n\t"
                    "bne\t%0, %1, %2\n\t"
                    "lui\t$2, %%hi(Fs_LoadPhase)\n\t"
                    ".set\treorder"
                    :
                    : "r"(st), "r"(one), "i"(&&case1_phase2)
                    : "v0");
            }
            endFlag = Fs_ChunkEndFlag;
            __asm__ volatile(
                ".set\tnoreorder\n\t"
                "bne\t%0, %1, %2\n\t"
                "lui\t$2, %%hi(Fs_LoadPhase)\n\t"
                "j\t%3\n\t"
                "sb\t%0, %%lo(Fs_LoadPhase)($2)\n\t"
                ".set\treorder"
                :
                : "r"(endFlag), "r"(ff), "i"(&&ret0), "i"(&&ret1)
                : "v0", "memory");
        case1_phase2:
            __asm__ volatile(
                ".set\tnoreorder\n\t"
                "li\t$3, 2\n\t"
                "sb\t$3, %%lo(Fs_LoadPhase)($2)\n\t"
                "lui\t$2, %%hi(Fs_Streaming)\n\t"
                "j\t%0\n\t"
                "sb\t$4, %%lo(Fs_Streaming)($2)\n\t"
                ".set\treorder"
                :
                : "i"(&&ret0)
                : "v1", "memory");
        }

        case 2: {
            s32 ff;
            /* Force: bne phase3; delay lui LoadPhase (matches target) */
            __asm__ volatile(
                ".set\tnoreorder\n\t"
                "lui\t$2, %%hi(Fs_ChunkEndSector)\n\t"
                "lui\t$3, %%hi(Fs_ReqSector)\n\t"
                "lw\t$4, %%lo(Fs_ChunkEndSector)($2)\n\t"
                "lw\t$2, %%lo(Fs_ReqSector)($3)\n\t"
                "nop\n\t"
                "bne\t$4, $2, %0\n\t"
                "lui\t$3, %%hi(Fs_LoadPhase)\n\t"
                ".set\treorder"
                :
                : "i"(&&case2_phase3)
                : "v0", "v1", "a0", "memory");
            /* equal path */
            status = Fs_LoadImageChunk((FsImageChunk*)(Fs_CdSector.bytes + 0x10), 0) & 0xFF;
            ff     = 0xFF;
            if (status == ff) {
                goto soft_error;
            }
            if (status == 0x7F) {
                goto soft_error;
            }
            endFlag = Fs_ChunkEndFlag;
            __asm__ volatile(
                ".set\tnoreorder\n\t"
                "bne\t%0, %1, %2\n\t"
                "lui\t$2, %%hi(Fs_LoadPhase)\n\t"
                "j\t%3\n\t"
                "sb\t%0, %%lo(Fs_LoadPhase)($2)\n\t"
                ".set\treorder"
                :
                : "r"(endFlag), "r"(ff), "i"(&&ret0), "i"(&&ret1)
                : "v0", "memory");
        case2_phase3:
            __asm__ volatile(
                ".set\tnoreorder\n\t"
                "j\t%0\n\t"
                "li\t$2, 3\n\t"
                ".set\treorder"
                :
                : "i"(&&set_phase_stream)
                : "v0");
        }

        case 3: {
            register s32 mode asm("v1");
            mode = Fs_ChunkMode;
            if (mode == 1) {
                goto phase_ff;
            }
            __asm__ volatile(
                ".set\tnoreorder\n\t"
                "addiu\t$2, $0, 4\n\t"
                "beq\t%0, $2, %1\n\t"
                "lui\t$3, %%hi(Fs_ChunkWritePtr)\n\t"
                ".set\treorder"
                :
                : "r"(mode), "i"(&&phase_ff)
                : "v0", "v1");
            {
                register u32  wp_hi asm("v1");
                register s32* srcp;
                s32*          dstp;
                __asm__ volatile("" : "=r"(wp_hi));
                srcp = (s32*)(Fs_CdSector.bytes + 0x10 + s2);
                s2   = 0;
                __asm__("lw %0, %%lo(Fs_ChunkWritePtr)(%1)" : "=r"(dstp) : "r"(wp_hi));
                sp = srcp;
                do {
                    tmp = *sp;
                    sp++;
                    s2++;
                    *dstp = tmp;
                    dstp++;
                } while (s2 < 0x1FC);
            }
            Fs_ChunkWritePtr += 0x7F0;
            if ((u32)Fs_ReqSector >= (u32)Fs_ChunkEndSector) {
                __asm__ volatile(
                    ".set\tnoreorder\n\t"
                    "lui\t$2, %%hi(Fs_ChunkEndFlag)\n\t"
                    "lbu\t$3, %%lo(Fs_ChunkEndFlag)($2)\n\t"
                    "j\t%0\n\t"
                    "addiu\t$2, $0, 0xFF\n\t"
                    ".set\treorder"
                    :
                    : "i"(&&check_end_ff)
                    : "v0", "v1");
            }
            Fs_LoadPhase = 0;
            Fs_Streaming = 1;
            goto ret0;
        }

        case 4:
            if (Fs_ChunkMode == 1) {
                goto phase_ff;
            }
            s2 = 0;
            if (Fs_ChunkMode != 4) {
                goto case4_body;
            }
        phase_ff:
            Fs_LoadPhase = 0xFF;
            return 1;
        case4_body: {
            s32           one;
            u32           ade_hi;
            FsUnkADE8*    ade;
            u8*           pp;
            FsFolderSlot* sl;
            register u8*  qq asm("v1");
            u32           vtmp;
            __asm__(
                "lui %0, %%hi(Fs_CdSector)\n\t"
                "addiu %1, %0, %%lo(Fs_CdSector+0x10)"
                : "=&r"(vtmp), "=r"(pp));
            __asm__(
                "lui %0, %%hi(D_8006ADE8)\n\t"
                "addiu %1, %0, %%lo(D_8006ADE8)"
                : "=&r"(ade_hi), "=r"(ade));
            one = 1;
            __asm__(
                "lui %0, %%hi(D_8006C338)\n\t"
                "addiu %1, %0, %%lo(D_8006C338)"
                : "=&r"(vtmp), "=r"(sl));
            qq = pp + 0xC;
            do {
                sl->field_0 = *pp;
                sl->field_4 = *(s32*)(qq - 4);
                tmp         = *(s32*)qq;
                if (tmp != 0) {
                    __asm__("sh %0, %%lo(D_8006ADE8)(%1)" : : "r"(one), "r"(ade_hi));
                    ade->field_4 = *(u16*)(qq - 0xA);
                    ade->field_8 = *(u32*)qq;
                }
                qq += 0x10;
                pp += 0x10;
                s2++;
                sl++;
            } while (s2 < 0x32);
        }
            {
                register u32 ade asm("v0");
                u32          lph;
                __asm__(
                    "lui %0, %%hi(D_8006ADE8)\n\t"
                    "lui %1, %%hi(Fs_LoadPhase)"
                    : "=&r"(ade), "=r"(lph));
                __asm__ volatile(
                    ".set\tnoreorder\n\t"
                    "sh\t$0, %%lo(D_8006ADE8+2)(%0)\n\t"
                    "j\t%1\n\t"
                    "li\t$2, 4\n\t"
                    ".set\treorder"
                    :
                    : "r"(ade), "i"(&&set_phase_stream)
                    : "v0", "memory");
            }

        case 5: {
            register s32 mode5 asm("v1");
            mode5 = Fs_ChunkMode;
            __asm__ volatile(
                ".set\tnoreorder\n\t"
                "addiu\t$2, $0, 3\n\t"
                "beq\t%0, $2, %1\n\t"
                "lui\t$3, %%hi(Fs_LoadPhase)\n\t"
                ".set\treorder"
                :
                : "r"(mode5), "i"(&&case5_join)
                : "v0", "v1");
            CdCmd_RequestVlcRebuild();
            __asm__ volatile(
                ".set\tnoreorder\n\t"
                "lui\t$2, %%hi(D_8006ADF8)\n\t"
                "move\t$4, $2\n\t"
                "lui\t$2, %%hi(Fs_ImgBuffers)\n\t"
                "lw\t$5, %%lo(Fs_ImgBuffers)($2)\n\t"
                "lui\t$2, %%hi(Fs_CdSector)\n\t"
                "addiu\t$6, $2, %%lo(Fs_CdSector)\n\t"
                "sw\t$0, %%lo(D_8006ADF8)($4)\n\t"
                "2:\n\t"
                "lw\t$3, %%lo(D_8006ADF8)($4)\n\t"
                "nop\n\t"
                "addu\t$2, $3, $6\n\t"
                "lbu\t$2, 16($2)\n\t"
                "addu\t$3, $5, $3\n\t"
                "sb\t$2, 0($3)\n\t"
                "lw\t$2, %%lo(D_8006ADF8)($4)\n\t"
                "nop\n\t"
                "addiu\t$2, $2, 1\n\t"
                "sw\t$2, %%lo(D_8006ADF8)($4)\n\t"
                "sltiu\t$2, $2, 2032\n\t"
                "bnez\t$2, 2b\n\t"
                "lui\t$3, 0x8007\n\t"
                ".set\treorder"
                :
                :
                : "v0", "v1", "a0", "a1", "a2", "memory");
        case5_join:
            /* shared by mode==3 and loop exit (after lui 0x8007 delay) */
            __asm__ volatile(
                ".set\tnoreorder\n\t"
                "j\t%0\n\t"
                "li\t$2, 5\n\t"
                ".set\treorder"
                :
                : "i"(&&set_phase_stream)
                : "v0");
        }

        case 6: {
            register s32 feed_r asm("v1");
            __asm__ volatile(
                ".set\tnoreorder\n\t"
                "lui\t$2, %%hi(Fs_ChunkMode)\n\t"
                "lbu\t$2, %%lo(Fs_ChunkMode)($2)\n\t"
                "nop\n\t"
                "addiu\t$2, $2, -4\n\t"
                "sltiu\t$2, $2, 2\n\t"
                "bnez\t$2, %0\n\t"
                "lui\t$3, %%hi(Fs_LoadPhase)\n\t"
                ".set\treorder"
                :
                : "i"(&&set_phase_ff)
                : "v0", "v1");

            SndLoad_BeginFromBuffer(0, Fs_CdSector.bytes);
            feed_r = SndLoad_FeedSector(Fs_CdSector.bytes);
            __asm__ volatile(
                ".set\tnoreorder\n\t"
                "li\t$2, 5\n\t"
                "bne\t%0, $2, %1\n\t"
                "li\t$2, -1\n\t"
                "lui\t$2, %%hi(Fs_ChunkEndFlag)\n\t"
                "lbu\t$3, %%lo(Fs_ChunkEndFlag)($2)\n\t"
                "j\t%2\n\t"
                "addiu\t$2, $0, 0xFF\n\t"
                ".set\treorder"
                :
                : "r"(feed_r), "i"(&&case6_not5), "i"(&&check_end_ff)
                : "v0", "v1", "memory");
        case6_not5:
            __asm__ volatile(
                ".set\tnoreorder\n\t"
                "bne\t%0, $2, %1\n\t"
                "lui\t$3, %%hi(Fs_LoadPhase)\n\t"
                ".set\treorder"
                :
                : "r"(feed_r), "i"(&&case6_phase6)
                : "v0", "v1");
        soft_error:
            Fs_OnCdError(0);
            return 0;
        case6_phase6:
            __asm__ volatile(
                ".set\tnoreorder\n\t"
                "j\t%0\n\t"
                "li\t$2, 6\n\t"
                ".set\treorder"
                :
                : "i"(&&set_phase_stream)
                : "v0");
        }

        case 7:
            __asm__ volatile(
                ".set\tnoreorder\n\t"
                "lui\t$2, %%hi(Fs_ChunkEndFlag)\n\t"
                "lbu\t$3, %%lo(Fs_ChunkEndFlag)($2)\n\t"
                "j\t%0\n\t"
                "addiu\t$2, $0, 0xFF\n\t"
                ".set\treorder"
                :
                : "i"(&&check_end_ff)
                : "v0", "v1");

        default: {
            register s32 req_live asm("a3");
            register s32 ef_live asm("a0");
            __asm__ volatile(
                ".set\tnoreorder\n\t"
                "bne\t%0, %1, %2\n\t"
                "lui\t$3, %%hi(Fs_LoadPhase)\n\t"
                "andi\t$3, %3, 0xFF\n\t"
                "addiu\t$2, $0, 0xFF\n\t"
                ".set\treorder"
                :
                : "r"(endSect), "r"(req_live), "i"(&&set_phase_ff), "r"(ef_live)
                : "v0", "v1");
        }
    }

check_end_ff:
    __asm__ volatile(
        ".set\tnoreorder\n\t"
        "bne\t$3, $2, %0\n\t"
        "lui\t$2, %%hi(Fs_LoadPhase)\n\t"
        "sb\t$3, %%lo(Fs_LoadPhase)($2)\n\t"
        ".set\treorder"
        :
        : "i"(&&ret0)
        : "v0", "memory");
ret1:
    return 1;

set_phase_ff:
    __asm__ volatile("li $2, 0xFF" ::: "v0");
set_phase_stream:
    __asm__ volatile("sb $2, %%lo(Fs_LoadPhase)($3)" ::: "memory");
    Fs_Streaming = 1;
ret0:
    return 0;
}

u8 Fs_ProcessChunkData(void)
{
    s32  status;
    s32  result;
    s32  endFlag;
    s32* offsets;
    s32  ff;
    u8*  val;

    switch (Fs_LoadPhase) {
        case 0:
            CdGetSector(Fs_ChunkWritePtr, 0x200);
            Fs_ChunkWritePtr += 0x800;
            if ((u32)Fs_ReqSector < (u32)Fs_ChunkEndSector) {
                goto ret0;
            }

        check_end_flag:
            endFlag = Fs_ChunkEndFlag;
            if (endFlag == 0xFF) {
                goto set_phase_done;
            }
            goto clear_streaming;

        case 1:
            CdGetSector(Fs_CdSector.bytes, 0x200);
            Fs_ChunkReadPtr = Fs_CdSector.bytes;
            Fs_DecompressChunk();
            if (D5B498_8006D748 == 0xFFFF) {
                goto soft_error;
            }
            if (D5B498_8006D748 == 0) {
                if ((u32)Fs_ReqSector < (u32)Fs_ChunkEndSector) {
                    goto ret0;
                }
            }
            switch (D5B498_8006ADF4 - 1) {
                case 0:
                    D_8006D860 = Fs_ChunkWritePtr - (u8*)D_8005C36C;
                    goto check_end_flag;
                case 1:
                    D_8006D864 = Fs_ChunkWritePtr - (u8*)D_8005C370;
                    goto check_end_flag;
                case 2:
                case 7:
                    D_8006D868 = Fs_ChunkWritePtr - (u8*)D_8005C374;
                    goto check_end_flag;
                case 3:
                    offsets    = &D_8006D860;
                    offsets[1] = -1;
                    D_8006D860 = Fs_ChunkWritePtr - (u8*)D_8005C36C;
                    goto check_end_flag;
                case 4: {
                    s32* p;
                    p          = &D_8006D860;
                    p[1]       = -1;
                    p[2]       = -1;
                    D_8006D860 = Fs_ChunkWritePtr - (u8*)D_8005C36C;
                    goto check_end_flag;
                }
                case 5:
                case 6:
                    goto check_end_flag;
            }
            goto check_end_flag;

        case 2:
            CdGetSector(Fs_CdSector.bytes, 0x200);
            status = Fs_LoadImageStrip(0) & 0xFF;
            ff     = 0xFF;
            if (status == ff) {
                goto hard_error;
            }
            if (status == 0x7F) {
                goto soft_error;
            }
            if (status != 1) {
                if ((u32)Fs_ReqSector < (u32)Fs_ChunkEndSector) {
                    goto ret0;
                }
            }
        check_end_a0:
            endFlag = Fs_ChunkEndFlag;
            if (endFlag == ff) {
                goto set_phase_done;
            }
            goto clear_streaming;

        case 3:
            CdGetSector(D_8006CCD8, 0x200);
            status = Fs_LoadImageChunk((FsImageChunk*)(D_8006CCD8 - 0x7F0), 0) & 0xFF;
            ff     = 0xFF;
            if (status != ff) {
                goto case3_ok;
            }
        hard_error:
            Fs_ReqSector -= 1;
            Fs_OnCdError(2);
            goto ret0;
        case3_ok:
            if (status == 0x7F) {
                goto soft_error;
            }
            goto check_end_a0;

        case 4:
            CdGetSector(Fs_ChunkWritePtr, 0x200);
            Fs_ChunkWritePtr += 0x800;
            if ((u32)Fs_ReqSector >= (u32)Fs_ChunkEndSector) {
                endFlag = Fs_ChunkEndFlag;
                if (endFlag == 0xFF) {
                    goto set_phase_done;
                }
                Fs_Streaming = 0;
            }
            if (D_8006ADE8.field_0 == 0) {
                goto ret0;
            }
            D_8006ADE8.field_2 += 1;
            if (D_8006ADE8.field_4 != D_8006ADE8.field_2) {
                goto ret0;
            }
            val = (u8*)D_8006ADE8.field_8;
            __asm__ volatile(
                ".set\tnoreorder\n\t"
                "lui $2, %%hi(Fs_ChunkWritePtr)\n\t"
                "j %1\n\t"
                "sw %0, %%lo(Fs_ChunkWritePtr)($2)\n\t"
                ".set\treorder"
                :
                : "r"(val), "i"(&&ret0)
                : "v0", "memory");

        case 5:
            if (Fs_ChunkMode != 3) {
                CdGetSector((u8*)Fs_ImgBuffers + D_8006ADF8, 0x200);
                D_8006ADF8 += 0x800;
            }
            if ((u32)Fs_ReqSector < (u32)Fs_ChunkEndSector) {
                goto ret0;
            }
            if (Fs_ChunkMode == 3) {
                goto check_end_flag;
            }
            Mdec_BeginDecode(Fs_ImgBuffers);
            goto check_end_flag;

        case 6:
            CdGetSector(Fs_CdSector.bytes, 0x200);
            result = SndLoad_FeedSector(Fs_CdSector.bytes);
            if (result != 5) {
                goto case6_not5;
            }
            endFlag = Fs_ChunkEndFlag;
            if (endFlag != 0xFF) {
                goto clear_streaming;
            }
        set_phase_done:
            Fs_LoadPhase = endFlag;
            return 1;

        case6_not5:
            if (result != -1) {
                goto ret0;
            }
        soft_error:
            Fs_OnCdError(0);
            goto ret0;

        default:
            if (Fs_ChunkEndSector != Fs_ReqSector) {
                goto ret0;
            }
            endFlag = Fs_ChunkEndFlag;
            if (endFlag != 0xFF) {
                goto clear_streaming;
            }
            return 1;
    }

clear_streaming:
    Fs_Streaming = 0;

ret0:
    return 0;
}

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
        CdReadyCallback(Fs_ReadNReadyCb);
        Fs_SeekSector = 0;
    } else {
        CdIntToPos(sector, loc);
        CdControlF(CdlReadN, &loc[0].minute);
        CdSyncCallback(Fs_ReadNReadyCb);
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
    SOFT_BARRIER();
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
        CdReadyCallback(Fs_ReadNReadyCb);
        Fs_SeekSector = 0;
    } else {
        CdIntToPos(sector, loc);
        CdControlF(CdlReadN, &loc[0].minute);
        CdSyncCallback(Fs_ReadNReadyCb);
        Fs_SeekSector = 0;
    }
}

void Fs_BuildFolderTables(s32 arg0, s32 arg1, s32 arg2)
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
        SOFT_BARRIER();
        dst             = (u8*)(((j & 0xFFFF) * 0x28) + (s32)destBase);
        stream->offset += files->offset + *table;
        for (k = 0; (u16)k < 0x28; k++) {
            dst[k & 0xFFFF] = src[k & 0xFFFF];
        }
        j += 1;
        goto loop_streams;
    }
}

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

                case 1: {
                    register FsCdfFileSmall* tbl asm("v0");
                    isValidCategory = true;
                    tbl             = Fs_FileTableCat1;
                    i               = Fs_FileTableCat1Len;
                    Fs_FileTableCat1Len++;
                    tbl[i].id     = fileId - 10000;
                    tbl[i].offset = ((FsCdfFile*)entry)->offset;
                    break;
                }

                case 2: {
                    register FsCdfFileSmall* tbl asm("v0");
                    isValidCategory = true;
                    tbl             = Fs_FileTableCat2;
                    i               = Fs_FileTableCat2Len;
                    Fs_FileTableCat2Len++;
                    tbl[i].id     = fileId - fileCategory * 10000;
                    tbl[i].offset = ((FsCdfFile*)entry)->offset;
                    break;
                }

                case 3: {
                    register FsCdfFileSmall* tbl asm("v0");
                    isValidCategory = true;
                    tbl             = Fs_FileTableCat3;
                    i               = Fs_FileTableCat3Len;
                    Fs_FileTableCat3Len++;
                    tbl[i].id     = fileId - 30000;
                    tbl[i].offset = ((FsCdfFile*)entry)->offset;
                    break;
                }

                case 4: {
                    register FsCdfFileSmall* tbl asm("v0");
                    isValidCategory = true;
                    tbl             = Fs_FileTableCat4;
                    i               = Fs_FileTableCat4Len;
                    Fs_FileTableCat4Len++;
                    tbl[i].id     = fileId - fileCategory * 10000;
                    tbl[i].offset = ((FsCdfFile*)entry)->offset;
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

            {
                register u32 flag asm("v0");
                flag = isValidCategory;
                if (!flag) {
                    u32                 v;
                    register FsCdfFile* tbl asm("v1");
                    words = sectorBuffer->words;
                    v     = words[(u16)headerOffset];
                    if (v / 100000 != 0) {
                        tbl = Fs_FileTable;
                        i   = Fs_FileTableLen;
                        Fs_FileTableLen++;
                        tbl[i].id = v;
                        tbl[i].offset =
                            ((FsCdfFile*)&words[(u16)headerOffset])->offset;
                    }
                }
            }

            headerOffset += sizeof(FsCdfFile) / sizeof(u32);
        }
    }

on_error:
    Fs_OnCdError(FS_ERROR_SOFT);
}

/* ISO directory name suffixes / special files (must sit in .rodata before ScanIso jtbl). */
const char D_800132F4[] = ".CDF";
const char D_800132FC[] = ".STR";
const char D_80013304[] = "STAGE0.HED";
const char D_80013310[] = "INIT.BS";

void Fs_ScanIsoDirectory(s32 mode)
{
    CdlLOC       loc[2];
    register u8* entry asm("s0");
    u8*          dest;
    s32          initBsSector;
    s32          initBsCount;
    s32          sector;
    s32          done;
    s32          i;
    s32          hasDot;
    u8*          namePtr;
    u8           idx;
    s32          c;
    s32          phase;
    s32          endSec;
    u8*          p;
    u8**         slot;

    initBsSector = 0;
    initBsCount  = initBsSector;

    SetMem(2);

restart:
    dest = (u8*)&Fs_CdSector;
    {
        s32 sync;
        sync   = CdSync(1, NULL);
        sector = 0x16;
        if (sync == CdlDiskError) {
            Fs_WaitDiskReset(true);
        }
    }

    Fs_CdOpStatus     = 0;
    Fs_ChunkEndFlag   = 0xFF;
    Fs_LoadPhase      = 0;
    Fs_ReqSector      = sector;
    Fs_ChunkEndSector = sector;
    Fs_ChunkWritePtr  = dest;
    Fs_VBlank         = VSync(-1);

    if (Fs_SeekSector == sector) {
        CdControlF(CdlReadN, NULL);
        CdReadyCallback(Fs_ReadNReadyCb);
        Fs_SeekSector = 0;
    } else {
        CdIntToPos(sector, loc);
        CdControlF(CdlReadN, (u8*)loc);
        CdSyncCallback(Fs_ReadNReadyCb);
        Fs_SeekSector = 0;
    }

    if (Fs_CdOpStatus != 0xFF) {
        do {
            if (Fs_CdOpStatus == 0x80) {
                Fs_ClearDiskError();
                goto restart;
            }
            VSync(0);
        } while (Fs_CdOpStatus != 0xFF);
    }

    entry = (u8*)&Fs_CdSector;
    if (entry[0] != 0x30 || entry[1] != 0) {
        {
            s32 sync;
            sync   = CdSync(1, NULL);
            sector = 0x14;
            if (sync == CdlDiskError) {
                Fs_WaitDiskReset(true);
            }
        }

        Fs_CdOpStatus     = 0;
        Fs_ChunkEndFlag   = 0xFF;
        Fs_LoadPhase      = 0;
        Fs_ReqSector      = sector;
        Fs_ChunkEndSector = sector;
        Fs_ChunkWritePtr  = entry;
        Fs_VBlank         = VSync(-1);

        if (Fs_SeekSector == sector) {
            CdControlF(CdlReadN, NULL);
            CdReadyCallback(Fs_ReadNReadyCb);
            Fs_SeekSector = 0;
        } else {
            CdIntToPos(sector, loc);
            CdControlF(CdlReadN, (u8*)loc);
            CdSyncCallback(Fs_ReadNReadyCb);
            Fs_SeekSector = 0;
        }

        if (Fs_CdOpStatus != 0xFF) {
            if (1) {
                do {
                    if (Fs_CdOpStatus == 0x80) {
                        Fs_ClearDiskError();
                        goto restart;
                    }
                    VSync(0);
                } while (Fs_CdOpStatus != 0xFF);
            }
        }
    }

    idx = 0;
    do {
        Fs_StageCdfSectors[idx] = 0;
        idx++;
    } while (idx < 6);

    {
        s32 hi;
        __asm__ volatile(
            "lui %0, %%hi(Fs_CdSector)\n\t"
            "addiu %1, %0, %%lo(Fs_CdSector)"
            : "=&r"(hi), "=r"(entry));
    }
    done                 = 0;
    D_8006AC30.field_4   = 0;
    Wip_SysFlags.field_0 = 0;
    D_8006AC30.sector    = 0;
    Fs_Stage0HedSector   = 0;

    while ((done & 0xFF) == 0) {
        switch (entry[0]) {
            case 0x38:
            case 0x3A:
            case 0x3C:
            case 0x3E:
                i      = 0;
                hasDot = 0;
                while (1) {
                    c = entry[0x21 + (i & 0xFF)];
                    if (c == 0x2E) {
                        hasDot = 1;
                        break;
                    }
                    if (c == 0) {
                        break;
                    }
                    i++;
                }
                if ((hasDot & 0xFF) != 0) {
                    {
                        s32   nameIdx;
                        char* s;
                        s       = D_800132F4;
                        nameIdx = i & 0xFF;
                        TOUCH_REG2(s, nameIdx);
                        namePtr = (entry + 0x21) + nameIdx;
                        if (strncmp(s, (char*)namePtr, 4) == 0) {
                            u8 stageNum;
                            stageNum = entry[0x20 + nameIdx] - 0x30;
                            Fs_StageCdfSectors[stageNum & 0xFF] =
                                *(u16*)(entry + 2) + (*(u16*)(entry + 4) << 16);
                        } else if (strncmp(D_800132FC, (char*)namePtr, 4) == 0) {
                            D_8006AC30.sector =
                                *(u16*)(entry + 2) + (*(u16*)(entry + 4) << 16);
                        } else if (strncmp(D_80013304, (char*)(entry + 0x21), 0xA) == 0) {
                            Fs_Stage0HedSector =
                                *(u16*)(entry + 2) + (*(u16*)(entry + 4) << 16);
                        } else if (strncmp(D_80013310, (char*)(entry + 0x21), 7) == 0) {
                            initBsCount = 0x10;
                            initBsSector =
                                *(u16*)(entry + 2) + (*(u16*)(entry + 4) << initBsCount);
                        }
                    }
                }
            case 0x30:
            case 0x32:
            case 0x34:
            case 0x36:
                entry += entry[0];
                break;
            default:
                done = 1;
                break;
        }
    }

    if (Fs_StageCdfSectors[1] != 0 || Fs_StageCdfSectors[2] != 0) {
        Wip_SysFlags.field_0 = 1;
    }
    if (Fs_StageCdfSectors[4] != 0 || Fs_StageCdfSectors[5] != 0) {
        Wip_SysFlags.field_0 = 2;
    }

    Fs_ClearDiskError();

    if ((mode & 0xFF) != 0) {
        if (initBsSector != 0) {
            Fs_ChunkMode    = 0;
            D_8006ADF8      = 0;
            D5B498_8006EA1A = 0;
            D5B498_8006EBB0 = 0;
            D5B498_8006D850 = NULL;
            D5B498_8006D748 = 0;
            D5B498_8006D858 = 1;
            slot            = &D_8006C4D4;
            p               = (u8*)&Fs_CdSector;
            *slot           = p + 0x800;
            CdCmd_RequestVlcRebuild();

            {
                s32          sync2;
                register s32 endSecR asm("s1");
                phase   = 5;
                sync2   = CdSync(1, NULL);
                endSecR = initBsSector + initBsCount;
                if (sync2 == phase) {
                    Fs_WaitDiskReset(true);
                }

                Fs_CdOpStatus     = 0;
                Fs_ChunkEndFlag   = 0xFF;
                Fs_LoadPhase      = phase;
                Fs_ReqSector      = initBsSector;
                Fs_ChunkEndSector = endSecR;
                Fs_ChunkWritePtr  = 0;
                Fs_VBlank         = VSync(-1);
            }

            if (Fs_SeekSector == initBsSector) {
                CdControlF(CdlReadN, NULL);
                CdReadyCallback(Fs_ReadNReadyCb);
                Fs_SeekSector = 0;
            } else {
                CdIntToPos(initBsSector, loc);
                CdControlF(CdlReadN, (u8*)loc);
                CdSyncCallback(Fs_ReadNReadyCb);
                Fs_SeekSector = 0;
            }

            if (Fs_CdOpStatus != 0xFF) {
                do {
                    do {
                        if (Fs_CdOpStatus != 0x80) {
                            VSync(0);
                        } else {
                            if (CdSync(1, NULL) != 5) {
                                goto restart;
                            }
                            Fs_WaitDiskReset(true);
                            goto restart;
                        }
                    } while (0);
                } while (Fs_CdOpStatus != 0xFF);
            }
            Fs_ClearDiskError();
        }
    }

    if (Fs_Stage0HedSector != 0) {
        Fs_CdOpStatus       = 0;
        Fs_FileTableLen     = 0;
        Fs_FileTableCat2Len = 0;
        Fs_FileTableCat4Len = 0;
        Fs_FileTableCat1Len = 0;
        Fs_FileTableCat3Len = 0;
        Fs_ReqSector        = Fs_Stage0HedSector;
        CdIntToPos(Fs_Stage0HedSector, loc);
        CdControlF(CdlReadN, (u8*)loc);
        CdReadyCallback(Fs_InitStage0TablesCb);
        Fs_VBlank = VSync(-1);
    } else if ((mode & 0xFF) != 0) {
        goto restart;
    } else {
        Wip_SysFlags.field_0 = 0;
    }
}

s32 Fs_LoadImageChunk(FsImageChunk* arg0, u8 arg1)
{
    u_long*       ot;
    register s32  retry asm("s1");
    s32           none;
    u8            yAdj;
    FsImageChunk* img;
    RECT*         rect;
    u_long*       z;
    u32           inRange;

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

    Fs_ChunkReadPtr  = (u8*)(arg0 + 1);
    rect             = &Fs_ImageRect;
    rect->w          = img->w;
    Fs_ChunkWritePtr = (u8*)D5B498_8006D870;
    rect->h          = img->h;
    Fs_DecompressImage();

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
        CLOBBER_REG($4);
    }

    D_8006C4C8[D5B498_8006ADF4] = (u8)Fs_ImageRect.h;
    Fs_ContinueDrawing(ot);
    return 0;
}

void Fs_CopyWorkEntries(FsWorkEntry* arg0)
{
    FsWorkEntry* base;
    register s32 term asm("t1");
    register u32 ace_hi asm("a3");
    FsWorkEntry* src;
    FsWorkEntry* dst;
    u32*         mid;

    src  = arg0;
    term = 0xFFFF;
    /* Keep %hi(Fs_WorkEntries) in $a3 across the copy loop so later
       field_0 loads can use lhu %lo(a3) (GCC will not CSE this itself). */
    __asm__(
        "lui %0, %%hi(Fs_WorkEntries)\n\t"
        "addiu %1, %0, %%lo(Fs_WorkEntries)"
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
        __asm__("lhu %0, %%lo(Fs_WorkEntries)(%1)" : "=r"(t) : "r"(ace_hi));
        c   = (s8)D5B498_8006C233 * 64;
        *px = t + c;
    } else {
        register s32 t asm("v1");
        s16*         px;
        px = &Fs_ImageRect.x;
        __asm__("lhu %0, %%lo(Fs_WorkEntries)(%1)" : "=r"(t) : "r"(ace_hi));
        *px = t;
    }

    if (Fs_ChunkMode == 2) {
        Fs_ImageRect.y = Fs_WorkEntries[0].field_2 + 0x80;
    } else {
        Fs_ImageRect.y = Fs_WorkEntries[0].field_2;
    }

    Fs_ImageRect.w  = 0x40;
    Fs_ImageRect.h  = 0x20;
    D5B498_8006ACD4 = 0x100;

    Fs_ChunkReadPtr = (u8*)arg0 + Fs_WorkEntries[0].field_4;

    if (Fs_WorkEntries[1].field_0 == 0xFFFF) {
        if (Fs_WorkEntries[1].field_2 == Fs_WorkEntries[1].field_0) {
            D5B498_8006ACD4 = 0x40;
        } else if (Fs_WorkEntries[1].field_2 & 0x8000) {
            D5B498_8006ACD4 = Fs_WorkEntries[1].field_2 & 0x7FFF;
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

void Fs_RetryReadN(void)
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
        CdSyncCallback(Fs_ReadNSyncCb);
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
        SOFT_BARRIER();
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
        CdReadyCallback(Fs_ReadNReadyCb);
        Fs_SeekSector = 0;
    } else {
        CdIntToPos(sector, loc);
        CdControlF(CdlReadN, &loc[0].minute);
        CdSyncCallback(Fs_ReadNReadyCb);
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
        CdSyncCallback(Fs_ReadNSyncCb);
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

void Fs_ReadNSyncCb(u8 status, u8* result)
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
        Fs_OnCdError(FS_ERROR_SOFT);
    }
}

void Fs_ReadNReadyCb(u8 status, u8* result)
{
    if (status != CdlDiskError) {
        Fs_VBlank       = VSync(-1);
        Fs_CdErrorCount = 0;
        Fs_Streaming    = 1;
        CdReadyCallback(Fs_CdReadyCb);
        CdSyncCallback(NULL);
    } else {
        Fs_OnCdError(FS_ERROR_SOFT);
    }
}

void Fs_SeekToPosCb(u8 status, u8* result)
{
    if (status == CdlComplete) {
        Fs_CdOpStatus = FS_CD_STATUS_IDLE;
        CdSyncCallback(NULL);
        Fs_CdErrorCount = 0;
    } else {
        Fs_OnCdError(FS_ERROR_SOFT);
    }
}

void Fs_OnCdError(u8 arg0)
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

void Fs_CheckReadTimeout(void)
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

s32 Fs_GetStageDiskKind(void)
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
