#include "common.h"

#include <psyq/libcd.h>
#include <psyq/libspu.h>

#include "main/display.h"
#include "main/unknown_syms.h"

s32 func_800567E4(void)
{
    volatile CdAudioPhase* p;
    volatile CdAudioLocEx* audio;
    CdStreamParams*        setup;
    CdlLOC*                loc;
    s32                    acc;
    register s32           i asm("s2");
    s32                    voice;
    s32                    status;
    s16                    half;
    s32                    two;
    s32                    temp;
    register s32           next asm("v0");
    void*                  sector;

    setup = &CdStream_Params;
    switch (CdAudio_Phase.field_0) {
        case 4:
            CdAudio_Ctl.field_10 = 0;
            acc                  = 0;
            i                    = 0x16;
            do {
                voice = (s8)i;
                temp  = acc + ((s32 (*)(s32))Spu_GetVoiceStatus)(voice);
                acc   = temp;
                asm("" : "+r"(temp));
                status = (s8)temp;
                if (status != 0) {
                    Spu_KeyOff(voice);
                }
                next = i + 1;
                i    = next;
                asm("" : "+r"(next));
            } while ((s8)next < 0x18);
            if (status != 0) {
                break;
            }
            CdAudio_Phase.field_0 = 1;
            /* fallthrough */
        case 1:
            p = &CdAudio_Phase;
            if (p->field_5 == 1) {
                p->field_2            = 4;
                CdAudio_Phase.field_0 = 3;
                break;
            }
            audio = (volatile CdAudioLocEx*)&CdAudio_Loc;
            loc   = (CdlLOC*)&audio->field_10;
            CdIntToPos(audio->field_C, loc);
            if (D_8008277C != 0) {
                half = 0;
            } else {
                half = audio->field_2;
            }
            two                = 2;
            setup->voiceR      = -1;
            setup->voiceL      = -1;
            setup->pitch       = half;
            setup->mode        = two;
            sector             = &Fs_CdSector;
            setup->sectorBuf   = sector;
            setup->spuBase     = audio->field_8;
            setup->startSector = CdPosToInt(loc);
            setup->doneCb      = func_80057D24;
            setup->startCb     = 0;
            setup->voiceFreeCb = 0;
            audio->field_1     = 0;
            CdStream_Start(setup);
            CdAudio_Phase.field_0 = two;
            break;
        case 2:
            if (CdAudio_Loc.field_1 != 0) {
                p = &CdAudio_Phase;
                if (p->field_5 == 1) {
                    p->field_2 = 4;
                }
                CdAudio_Phase.field_0 = 3;
                CdAudio_Loc.field_1   = 0;
            }
            break;
        case 0:
        case 3:
            break;
    }
    return 1;
}

/* Alignment pad after func_800567E4's 5-entry jump table so CdAudio_DriveSeek's
 * compiler-generated jtbl lands at 0x800141DC. */
static const s32 s_jtbl_pad_567E4 = 0;

s32 CdAudio_DrivePhase0(void)
{
    volatile CdAudioPhase*  p;
    s16                     ret;
    LinInterp*              interp;
    volatile CdAudioLocEx*  parent;
    volatile CdAudioVoices* voices;

    p   = &CdAudio_Phase;
    ret = 3;

    switch (p->field_2) {
        case 1:
            interp     = &LinInterp_CdStream;
            p->field_1 = 4;
            LinInterp_Step(interp);
            if (LinInterp_CdStream.field_0 == interp->field_4) {
                interp->field_E = 0;
                CdStream_SetPitch(0);
                CdStream_Stop();
                p->field_2 = 2;
            } else {
                parent = (volatile CdAudioLocEx*)interp - 1;
                CdStream_SetPitch((s16)LinInterp_Apply(interp, parent->field_2));
            }
            break;
        case 2:
            voices = (volatile CdAudioVoices*)&CdAudio_Loc;
            Spu_KeyOff(voices->field_3E);
            Spu_KeyOff(voices->field_3F);
            p->field_2 = 3;
            /* fallthrough */
        case 3:
            if (CdStream_IsBusy() == 0) {
                CdAudio_Phase.field_2 = 4;
                ret                   = 0;
            }
            break;
    }
    return ret;
}

typedef struct {
    /* 0x0 */ u8 pad[2];
    /* 0x2 */ u8 field_2;
    /* 0x3 */ u8 field_3;
} SectorHdr;

void CdAudio_ReadyCallback(s32 arg0);
void CdAudio_FeedSector(s32 arg0);

s32 CdAudio_DriveSeek(void)
{
    u8                   phase;
    register SectorHdr*  hdr asm("a2");
    volatile CdAudioCtl* stream;
    s32                  status;
    register s32         tmp asm("a0");
    s32                  val;
    register s32         ptr asm("v1");
    u8                   idx;
    volatile CdAudioLoc* audio;
    volatile CdAudioTbl* cd;

    phase = CdAudio_Phase.field_3;
    hdr   = (SectorHdr*)D_80082750;

    switch (phase) {
        case 5:
        do_setloc:
            CdAudio_Ctl.field_0   = 0;
            CdAudio_Phase.field_3 = 1;
            CdControlF(CdlSetloc, D_800827B0);
            break;
        case 1:
            stream = &CdAudio_Ctl;
            if (stream->field_0 < 0x259) {
                if (CdSync(1, NULL) == CdlDiskError) {
                    CdFlush();
                    goto do_setloc;
                }
                CdAudio_Phase.field_3 = 6;
                    /* fallthrough */
                case 6:
                    CdAudio_Phase.field_3 = 3;
                    CdReadyCallback(CdAudio_ReadyCallback);
                    CdAudio_Ctl.field_0 = 0;
                    D_80082770          = 0;
                    CdAudio_Ctl.field_A = 0;
                    CdControlF(CdlReadN, NULL);
                    break;
            }
            goto timeout;
        case 3:
            if (CdSync(1, NULL) == CdlDiskError) {
                CdAudio_Phase.field_3 = 6;
                CdFlush();
                CdReadyCallback(NULL);
            } else {
                CdAudio_Phase.field_3 = 8;
                CdAudio_Ctl.field_4   = 0;
            }
            break;
        case 8:
            stream = &CdAudio_Ctl;
            if ((u8)stream->field_A != 0) {
                stream->field_8 = CdAudio_Phase.field_3;
                stream->field_9 = 2;
                goto error;
            }
            if (stream->field_0 < 0x259) {
                if (D_80082770 != 0) {
                    audio          = &CdAudio_Loc;
                    idx            = hdr->field_3;
                    val            = D_80068B18[idx];
                    ptr            = D_80082750;
                    audio->field_8 = val;
                    cd             = &CdAudio_Tbl;
                    idx            = hdr->field_2;
                    cd->field_C    = (u16*)(ptr + (idx * 4));
                    CdReadyCallback(NULL);
                    CdAudio_Phase.field_3 = 9;
                        /* fallthrough */
                    case 9:
                        CdControlF(CdlPause, NULL);
                        CdAudio_Ctl.field_0   = 0;
                        CdAudio_Phase.field_3 = 0xA;
                        /* fallthrough */
                    case 10:
                        stream = &CdAudio_Ctl;
                        if (stream->field_0 < 0x259) {
                            goto do_cdsync;
                        }
                } else {
                    break;
                }
            }
        timeout:
            stream->field_8 = CdAudio_Phase.field_3;
            stream->field_9 = 1;
            goto error;
        do_cdsync:
            status = CdSync(1, NULL);
            if (status == CdlComplete) {
                goto set_state_4;
            }
            if (status < 3) {
                break;
            }
            if (status != CdlDiskError) {
                break;
            }
            CdFlush();
        set_state_4:
            CdAudio_Phase.field_3 = 4;
            break;
        default:
            break;
    }

    tmp                 = CdAudio_Ctl.field_0;
    tmp                 = tmp + 1;
    CdAudio_Ctl.field_0 = tmp;
    return 5;

error:
    CdAudio_Phase.field_3 = 0x80;
    return 0;
}

s32 CdAudio_DriveRead(void)
{
    volatile CdAudioVoices* voices;
    volatile CdAudioCtl*    stream;
    volatile CdAudioCtl*    p;
    volatile CdAudioLocEx*  audio;
    volatile CdAudioTbl*    cd;
    s32                     status;
    u8                      mode;
    s32                     ret;

    switch (CdAudio_Phase.field_4) {
        case 1:
            voices = (volatile CdAudioVoices*)&CdAudio_Loc;
            Spu_KeyOff(voices->field_3E);
            Spu_KeyOff(voices->field_3F);
            if (CdStream_IsBusy() != 0) {
                break;
            }
        do_setmode:
            CdAudio_Phase.field_4 = 2;
            CdAudio_Ctl.field_0   = 0;
            mode                  = CdlModeSpeed | CdlModeSize1;
            CdControlF(CdlSetmode, &mode);
            break;
        case 2:
            stream = &CdAudio_Ctl;
            if (stream->field_0 < 0x259) {
                goto case2_sync;
            }
            stream->field_8 = CdAudio_Phase.field_4;
            stream->field_9 = 1;
            goto error;
        case2_sync:
            status = CdSync(1, NULL);
            if (status == CdlComplete) {
                goto case2_ok;
            }
            if (status < 3) {
                break;
            }
            if (status != CdlDiskError) {
                break;
            }
            CdFlush();
            goto do_setmode;
        case2_ok:
            stream->field_C       = 5;
            CdAudio_Phase.field_4 = 3;
            break;
        case 3:
            CdAudio_Ctl.field_C = CdAudio_Ctl.field_C - 1;
            if (CdAudio_Ctl.field_C >= 0) {
                break;
            }
            CdAudio_Phase.field_4 = 4;
            break;
        case 4:
        do_setloc:
            CdAudio_Ctl.field_0 = 0;
            CdAudio_Tbl.field_8 = ((volatile CdAudioLocEx*)&CdAudio_Loc)->field_4;
            audio               = (volatile CdAudioLocEx*)&CdAudio_Loc;
            CdIntToPos(audio->field_4, (CdlLOC*)&audio->field_10);
            CdAudio_Phase.field_4 = 5;
            CdControlF(CdlSetloc, (u8*)&audio->field_10);
            break;
        case 5:
            p = &CdAudio_Ctl;
            if (p->field_0 < 0x259) {
                status = CdSync(1, NULL);
                if (status == CdlComplete) {
                    goto case5_ok;
                }
                if (status < 3) {
                    break;
                }
                if (status != CdlDiskError) {
                    break;
                }
                CdFlush();
                goto do_setloc;
            case5_ok:
                CdAudio_Phase.field_4 = 6;
                break;
            }
            goto timeout;
        case 6:
            CdAudio_Phase.field_4 = 7;
            CdAudio_Tbl.field_1   = 0;
            CdAudio_Ctl.field_B   = 0;
            CdAudio_Ctl.field_0   = 0;
            SpuSetTransferStartAddr(CdAudio_Tbl.field_10);
            CdReadyCallback(CdAudio_FeedSector);
            CdControlF(CdlReadN, NULL);
            break;
        case 7:
            if (CdSync(1, NULL) == CdlDiskError) {
                CdAudio_Phase.field_4 = 6;
                CdFlush();
                CdReadyCallback(NULL);
            } else {
                CdAudio_Phase.field_4 = 8;
                CdAudio_Ctl.field_4   = 0;
            }
            break;
        case 8:
            p = &CdAudio_Ctl;
            if (p->field_B != 0) {
                p->field_8 = CdAudio_Phase.field_4;
                p->field_9 = 2;
                goto error;
            }
            if (CdAudio_Ctl.field_0 < 0x259) {
                if (p->field_4 < 0x259) {
                    cd = &CdAudio_Tbl;
                    if (cd->field_1 == 0) {
                        goto case8_inc;
                    }
                    if (cd->field_1 == 1) {
                        goto do_pause;
                    }
                    goto error;
                case8_inc:
                    p->field_4 = p->field_4 + 1;
                    break;
                }
                goto timeout;
            }
            goto timeout;
        case 11:
        do_pause:
            CdReadyCallback(NULL);
            CdAudio_Ctl.field_0   = 0;
            CdAudio_Phase.field_4 = 9;
            CdControlF(CdlPause, NULL);
            break;
        case 9:
            status = CdSync(1, NULL);
            if (status == CdlComplete) {
                goto case9_ok;
            }
            if (status < 3) {
                goto case9_check;
            }
            if (status != CdlDiskError) {
                goto case9_check2;
            }
            CdFlush();
            goto do_pause;
        case9_ok:
            ret                   = 0;
            CdAudio_Phase.field_4 = 0xA;
            CdAudio_Phase.field_2 = 4;
            return ret;
        case9_check:
            p = &CdAudio_Ctl;
        case9_check2:
            p = &CdAudio_Ctl;
            if (p->field_0 < 0x259) {
                break;
            }
        timeout:
            p->field_8 = CdAudio_Phase.field_4;
            p->field_9 = 1;
        error:
            CdReadyCallback(NULL);
            CdFlush();
            CdControlF(CdlPause, NULL);
            CdAudio_Phase.field_4 = 1;
            goto do_setmode;
        case 10:
        default:
            break;
    }

    CdAudio_Ctl.field_0 = CdAudio_Ctl.field_0 + 1;
    return 6;
}

void CdAudio_FeedSector(s32 arg0)
{
    s32                  arg;
    s32                  pos;
    s32                  ret;
    SndLoadState*        state;
    s32                  spuIdx;
    volatile CdAudioCtl* stream;
    volatile CdAudioTbl* cdState;
    FsSector*            sector;
    volatile CdAudioLoc* audio;

    sector = &Fs_CdSector;
    stream = &CdAudio_Ctl;
    if (stream->field_B != 0) {
        return;
    }
    cdState = &CdAudio_Tbl;
    if (cdState->field_1 != 0) {
        return;
    }
    arg = arg0 & 0xFF;
    if (arg != 1) {
        stream->field_B = 2;
        return;
    }
    CdGetSector(sector, 3);
    pos = CdPosToInt((CdlLOC*)sector);
    if (cdState->field_8 != pos) {
        cdState->field_1 = 0xFF;
        stream->field_B  = arg;
        return;
    }
    CdGetSector(sector, 0x200);
    audio = &CdAudio_Loc;
    if (audio->field_4 == cdState->field_8) {
        state           = &SndLoad_State;
        state->field_0  = 0x10;
        state->field_26 = 0;
        state->field_1  = 0;
        state->field_3  = 0;
        state->field_10 = 0x7C0;
        state->field_2  = 4;
        state->field_C  = sector->words[1];
        state->field_28 = sector->bytes[0];
        spuIdx          = sector->words[2];
        if (spuIdx != 0) {
            SpuSetTransferStartAddr(D_80068B2C[spuIdx]);
        }
        if (SndLoad_ProcessSector(&sector->bytes[0x40]) == 7) {
            stream->field_B = 3;
            return;
        }
        state->field_10 = 0x800;
    } else {
        ret = SndLoad_ProcessSector(sector);
        if (ret == 5) {
            cdState->field_1 = arg;
            CdReadyCallback(0);
        } else if (ret == 7) {
            stream->field_B = 4;
        }
    }
    CdAudio_Tbl.field_8 += 1;
}

void CdAudio_Init(void)
{
    u32  i;
    s32* p;

    p = (s32*)&CdAudio_Phase;
    i = 0;
    do {
        i++;
        *p = 0;
    } while (i < 2U);

    p = (s32*)&CdAudio_Loc;
    i = 0;
    do {
        i++;
        *p = 0;
    } while (i < 0x11U);

    D_8008277C          = 0;
    D_80082750          = 0;
    CdAudio_Loc.field_8 = 0x51010;
    Spu_SetVoiceRange(3, 0x16, 2);
    CdStream_Reset();
    func_800260B0(1);
}

u8 CdAudio_GetState(void)
{
    return CdAudio_Loc.field_0;
}

void CdAudio_Tick(void)
{
    if ((CdAudio_Loc.field_4 != 0) && (CdAudio_Loc.field_0 != 0)) {
        CdAudio_Loc.field_0 = D_80068B34[CdAudio_Loc.field_0 & 7]();
        CdStream_Drive();
    }
}

s32 CdAudio_Reset(s32 arg0)
{
    volatile CdAudioCtl* p;

    D_800827E4          = 0;
    D_80082754          = 0;
    p                   = &CdAudio_Ctl;
    p->field_C          = 0;
    p->field_8          = 0;
    p->field_9          = 0;
    CdAudio_Loc.field_4 = arg0;
    D_80082750          = 0;
    return 0;
}

s32 CdAudio_SetupStream(void)
{
    u8                     mode;
    s32                    mem;
    s32                    buf;
    volatile CdAudioLocEx* p;

    CdAudio_Phase.field_3 = 5;
    p                     = (volatile CdAudioLocEx*)&CdAudio_Loc;
    CdIntToPos(p->field_4, (CdlLOC*)&p->field_10);
    buf = D_80082750;
    if (buf != 0) {
        F3D458_Free((void*)buf);
    }
    mem                 = (s32)F3D458_Malloc(0x800);
    D_80082750          = mem;
    D_80082778          = mem + 4;
    CdAudio_Loc.field_0 = 5;
    mode                = CdlModeSpeed | CdlModeSize1;
    CdControlB(CdlSetmode, &mode, NULL);
    return 0;
}

s32 func_800576BC(s32 arg0)
{
    s32 temp_s0;

    temp_s0 = arg0 & 0xFF;
    if (temp_s0 != 0) {
        func_80057A88(CdAudio_Loc.field_4 + CdAudio_LoadSectorEntry((arg0 - 1) & 0xFF));
    }
    return temp_s0;
}

s32 func_80057704(void)
{
    return func_80057ACC();
}

s32 func_80057724(void)
{
    CdAudioTblEntry* temp;
    s32              ret;

    if (CdAudio_Phase.field_0 != 3) {
        return -1;
    }
    if (CdAudio_Phase.field_2 != 0) {
        ret = 1;
    } else {
        temp = CdAudio_TblEntries + CdAudio_Tbl.field_2;
        CdAudio_LoadSectorEntry((temp[1].field_3 - temp->field_3 - 1) & 0xFF);
        CdAudio_StartVolumeRamp(0x20);
        ret = 0;
    }
    return ret;
}

s32 func_800577AC(s32 arg0, s32 arg1)
{
    if (CdStream_IsBusy() != 0) {
        return -1;
    }
    CdAudio_ResetKeepBuffer(arg0);
    CdAudio_Loc.field_2 = D_80068A80[arg1 & 0xFF] << 7;
    SndEvt_EnqueueType7(0x80000000, 0);
    return CdAudio_StoreIfNonNull(arg0);
}

s32 CdAudio_ResetKeepBuffer(s32 arg0)
{
    volatile CdAudioCtl*   p;
    volatile CdAudioLoc*   r;
    volatile CdAudioPhase* q;
    s32                    field4;

    D_800827E4 = 0;
    r          = &CdAudio_Loc;
    field4     = r->field_4;
    D_80082754 = 0;
    if (field4 == 0) {
        r->field_4 = arg0;
    }
    p           = &CdAudio_Ctl;
    p->field_C  = 0;
    p->field_8  = 0;
    p->field_9  = 0;
    p->field_10 = 1;
    r->field_8  = D_80068B1C;
    q           = &CdAudio_Phase;
    q->field_2  = 0;
    q->field_0  = 0;
    q->field_1  = 0;
    q->field_4  = 0;
    q->field_5  = 0;
    return 0;
}

s32 CdAudio_StoreIfNonNull(s32 arg0)
{
    if (arg0 != 0) {
        func_80057A88(arg0);
    }
    return arg0;
}

s32 func_800578C4(void)
{
    return func_80057ACC();
}

s32 func_800578E4(s32 arg0)
{
    if (arg0 == 0) {
        return -1;
    }
    func_80057B88(arg0, D_80082124);
    return 0;
}

void func_8005791C(s32 arg0)
{
    CdAudio_Loc.field_4 = arg0;
}

void CdAudio_CopyVoiceData(s8 arg0, s32* arg1)
{
    SpuVoiceRef sp10;
    s32*        dest;
    u32         i;

    Spu_SetVoiceCallbacks(arg0, 0, 0);
    Spu_GetVoiceRef(arg0, &sp10);
    dest = (s32*)sp10.field_4;
    i    = 0;
    do {
        *dest = *arg1;
        arg1++;
        i++;
        dest++;
    } while (i < 0x10U);
}

void CdAudio_AllocVoices(s8* arg0, s8* arg1)
{
    *arg0 = Spu_AllocVoice(D_80068B28, 3, 0xFFFF);
    *arg1 = Spu_AllocVoice(D_80068B28, 3, 0xFFFF);
    F3E48C_DisableVoice(*arg0);
    F3E48C_DisableVoice(*arg1);
}

s32 CdAudio_LoadSectorEntry(s32 arg0)
{
    u32  temp_v0;
    u16* table;

    temp_v0             = ((u32*)D_80082778)[D_80082754 + (arg0 & 0xFF)];
    CdAudio_Loc.field_2 = (temp_v0 >> 17) & 0x3F80;
    CdAudio_Tbl.field_0 = temp_v0 >> 31;
    table               = CdAudio_Tbl.field_C;
    CdAudio_Tbl.field_4 = table[((temp_v0 >> 14) & 0x3FC) / 2];
    return temp_v0 & 0xFFFF;
}

s32 func_80057A88(s32 arg0)
{
    s32 temp_v0;

    temp_v0 = CdAudio_Phase.field_1;
    if ((temp_v0 == 4) || (temp_v0 == 0)) {
        CdAudio_Loc.field_C   = arg0;
        CdAudio_Loc.field_0   = 1;
        CdAudio_Phase.field_0 = 4;
    }
    return 0;
}

s32 func_80057ACC(void)
{
    volatile CdAudioPhase* p;
    s32                    ret;

    p = &CdAudio_Phase;
    if (p->field_0 != 3) {
        ret        = -1;
        p->field_1 = 4;
        p->field_2 = 1;
    } else {
        ret                 = 0;
        p->field_2          = 0;
        p->field_1          = 1;
        CdAudio_Loc.field_0 = 2;
    }
    return ret;
}

void CdAudio_StartVolumeRamp(s32 arg0)
{
    LinInterp*             p;
    volatile CdAudioLocEx* parent;

    p      = &LinInterp_CdStream;
    parent = (volatile CdAudioLocEx*)p;
    parent = parent - 1;
    LinInterp_Setup(p, (parent->field_2 >> 7) & 0xFF, 0, arg0);
    CdAudio_Phase.field_1 = 4;
    CdAudio_Phase.field_2 = 1;
    parent->field_0       = 3;
}

void func_80057B88(s32 arg0, s32 arg1)
{
    CdAudio_Loc.field_4   = arg0;
    CdAudio_Tbl.field_10  = arg1;
    CdAudio_Phase.field_4 = 1;
    CdAudio_Loc.field_0   = 6;
}

s32 func_80057BC0(void)
{
    return 0;
}

s32 CdAudio_DrivePhase1(void)
{
    s16 ret;

    ret = 2;
    switch (CdAudio_Phase.field_1) {
        case 0:
            break;
        case 1:
        case 2:
            CdStream_ArmSpuIrq();
            CdAudio_Phase.field_1 = 3;
            break;
        case 3:
            if (CdStream_IsBusy() == 0) {
                ret                   = 3;
                CdAudio_Phase.field_1 = 4;
                CdAudio_Phase.field_2 = 2;
            }
            break;
        case 4:
            break;
    }
    return ret;
}

void CdAudio_ReadyCallback(s32 arg0)
{
    s32                  temp;
    s32                  pos;
    FsSector*            sector;
    volatile CdAudioLoc* state;

    if (D_80082770 != 0) {
        return;
    }

    temp = arg0 & 0xFF;
    if (temp == 1) {
        sector = &Fs_CdSector;
        CdGetSector(sector, 3);
        state = &CdAudio_Loc;
        pos   = CdPosToInt((CdlLOC*)sector);
        if (state->field_4 != pos) {
            CdAudio_Ctl.field_A = -2;
        }
        CdGetSector((void*)D_80082750, 0x200);
        D_80082770 = temp;
    } else {
        CdAudio_Ctl.field_A = -1;
    }
}

void func_80057D24(void)
{
    CdAudio_Loc.field_1 = 1;
}

s32 CdReady_Enqueue(CdReadyEntry* arg0)
{
    u8                     saved;
    s32                    field2;
    s32                    next;
    u32                    flags;
    s32                    temp;
    volatile CdReadyEntry* entry;
    volatile CdReadyQueue* p;

    p                    = &CdReady_Queue;
    saved                = CdReady_Queue.locked;
    CdReady_Queue.locked = 1;

    field2 = (s8)p->readIdx;
    next   = (s8)p->writeIdx;
    next   = next + 1;
    if (next >= 4) {
        next = 0;
    }

    if (next == field2) {
        CdReady_Queue.locked = saved;
        return 0;
    }

    entry                = &CdReady_Queue.entries[(s8)p->writeIdx];
    entry->pollFn        = arg0->pollFn;
    entry->sectorPos     = arg0->sectorPos;
    temp                 = arg0->doneFn;
    flags                = entry->flags;
    flags                = flags | 1;
    entry->doneFn        = temp;
    temp                 = ~4;
    flags                = flags & temp;
    flags                = flags & ~8;
    flags                = flags & ~0x1FE0;
    temp                 = arg0->errorFn;
    flags                = flags | 2;
    entry->flags         = flags;
    entry->errorFn       = temp;
    field2               = (s8)p->writeIdx;
    p->writeIdx          = next;
    CdReady_Queue.locked = saved;
    return field2 + 1;
}

void CdReady_Poll(void)
{
    volatile CdReadyQueue* p;
    CdReadyEntry*          entry;
    u32                    flags;

    p = &CdReady_Queue;
    if (p->locked == 0 && p->writeIdx != p->readIdx) {
        entry = (CdReadyEntry*)&CdReady_Queue.entries[(s8)p->readIdx];
        flags = entry->flags;
        if (flags & 1) {
            if (((s32 (*)(CdReadyEntry*))entry->pollFn)(entry) != 0) {
                if (entry->doneFn != 0) {
                    ((void (*)(void))entry->doneFn)();
                }
                entry->flags &= ~1;
                entry->flags &= ~4;
                p->readIdx    = p->readIdx + 1;
                if ((s8)p->readIdx >= 4) {
                    p->readIdx = 0;
                }
            }
        } else if (((flags >> 2) & 1) && !((flags >> 1) & 1)) {
            if (entry->errorFn != 0) {
                ((void (*)(CdReadyEntry*))entry->errorFn)(entry);
            }
            if (!((entry->flags >> 3) & 1)) {
                goto advance;
            }
        } else {
        advance:
            entry->flags         &= ~4;
            CdReady_Queue.readIdx = CdReady_Queue.readIdx + 1;
            if ((s8)CdReady_Queue.readIdx >= 4) {
                CdReady_Queue.readIdx = 0;
            }
        }
    }
}

void CdStream_Continue(void);

void CdStream_Start(CdStreamParams* arg0)
{
    CdReadyEntry                     entry;
    register volatile CdStreamState* p asm("s1");
    s32                              flag;
    volatile CdStreamState*          ap;
    volatile CdStreamState*          a3;
    CdStreamChannel*                 t0;
    CdStreamChannel*                 ch1;
    register s32                     sectors asm("a0");
    s16                              pitch;
    u8                               saved;
    s16                              f6;
    s16                              idx;
    u32                              flags;
    CdReadyEntry*                    e;
    s32                              one;
    s32                              cflags;
    s16                              vff;
    s16                              v1fc3;
    s16                              v1000;
    s32                              temp;
    u8                               mode;
    register s32                     base asm("v1");

    p         = &CdStream_State;
    p->voiceL = arg0->voiceL;
    p->voiceR = arg0->voiceR;
    flag      = ((u8)CdStream_State.flags0 >> 4) & 1;
    if (flag == 1) {
        Spu_KeyOff((s8)p->voiceL);
        Spu_KeyOff((s8)p->voiceR);
        if (arg0->voiceFreeCb != 0) {
            ((void (*)(s32))arg0->voiceFreeCb)(
                (flag << (s8)p->voiceL) | (flag << (s8)p->voiceR));
        }
    }
    SpuSetIRQ(0);
    SpuSetIRQCallback(NULL);
    SpuSetTransferCallback(NULL);
    CdSyncCallback(NULL);

    ap                     = &CdStream_State;
    *(s32*)&CdStream_State = 0;
    if (ap->readySlot != 0) {
        a3    = (volatile CdStreamState*)&CdReady_Queue;
        f6    = ap->readySlot;
        saved = CdReady_Queue.locked;
        if (f6 != 0) {
            idx   = f6 - 1;
            e     = (CdReadyEntry*)&CdReady_Queue.entries[idx];
            flags = e->flags;
            if (flags & 1) {
                e->flags = (flags & ~1) | 4;
            }
            CdReady_Queue.locked = saved;
        }
        CdStream_State.readySlot = 0;
    }

    a3              = &CdStream_State;
    a3->startCb     = (void (*)(s32))arg0->startCb;
    a3->voiceFreeCb = (void (*)(s32))arg0->voiceFreeCb;
    a3->field_4     = 0;
    a3->doneCb      = (void (*)(s32))arg0->doneCb;
    a3->field_18    = 0;
    a3->startSector = arg0->startSector;
    a3->field_2C    = arg0->startSector;
    one             = 1;
    a3->field_30    = arg0->startSector;
    a3->field_34    = 0;
    a3->field_38    = one;
    base            = arg0->spuBase;
    sectors         = 0x18;
    {
        s32 ds      = Display_State.field_124;
        a3->spuBase = base;
        if (ds == one) {
            sectors = 0x14;
        }
    }
    a3->sectorsPerChunk = sectors;
    a3->ringHalf        = 0x2770;
    t0                  = (CdStreamChannel*)(a3 + 1);
    a3->sector          = (MtsSector*)arg0->sectorBuf;
    a3->voiceL          = arg0->voiceL;
    vff                 = 0xFF;
    a3->voiceR          = arg0->voiceR;
    mode                = arg0->mode;
    v1fc3               = 0x1FC3;
    v1000               = 0x1000;
    cflags              = 0x6009F;
    t0->attr            = cflags;
    t0[1].attr          = cflags;
    t0->field_C         = 0;
    t0->field_E         = 0;
    t0->field_14        = v1000;
    t0->field_3A        = vff;
    t0->field_3C        = v1fc3;
    t0[1].field_C       = 0;
    t0[1].field_E       = 0;
    t0[1].field_14      = v1000;
    a3->mode            = mode;
    a3->field_1C        = 0;
    a3->field_20        = 0;
    a3->pending         = 0;
    t0->voiceMask       = one << a3->voiceL;
    t0->spuAddr         = a3->spuBase;
    {
        register s32 addr asm("v0");
        register s32 mask asm("v1");
        addr            = a3->spuBase;
        mask            = a3->voiceR;
        addr            = addr + 0x10;
        mask            = one << mask;
        t0->spuAddr2    = addr;
        t0[1].voiceMask = mask;
    }
    temp          = a3->spuBase;
    temp          = temp + 0x40;
    temp          = temp + ((s32)((u16)a3->ringHalf << 16) >> 15);
    t0[1].spuAddr = temp;
    temp          = a3->spuBase;
    {
        s32 shift      = (s32)((u16)a3->ringHalf << 16) >> 15;
        t0[1].field_3A = vff;
        t0[1].field_3C = v1fc3;
        temp           = temp + shift;
    }
    {
        u8 f53         = a3->flags;
        temp           = temp + 0x50;
        t0[1].spuAddr2 = temp;
        if (f53 & 2) {
            ch1           = (CdStreamChannel*)(a3 + 1) + 1;
            pitch         = (arg0->pitch * 0xB5) >> 8;
            ch1->pitchAlt = pitch;
            ch1->pitch    = pitch;
            t0->pitchAlt  = pitch;
            t0->pitch     = pitch;
        } else {
            u16 pitch_u;
            pitch_u        = *(u16*)&arg0->pitch;
            t0->pitchAlt   = 0;
            t0[1].pitch    = 0;
            t0->pitch      = pitch_u;
            t0[1].pitchAlt = *(u16*)&arg0->pitch;
        }
    }

    {
        register s32 rem_tmp asm("a0");
        register s32 temp_v1 asm("v1");

        rem_tmp                  = (s32)&entry;
        entry.pollFn             = (s32)func_80059EE0;
        temp_v1                  = arg0->startSector;
        entry.doneFn             = (s32)CdStream_Continue;
        entry.errorFn            = (s32)CdStream_FinishQueueEntry;
        entry.sectorPos          = temp_v1;
        CdStream_State.readySlot = CdReady_Enqueue((CdReadyEntry*)rem_tmp);
    }
    CdStream_State.phase = 2;
    D_80068B74           = -1;
}

void CdStream_Continue(void)
{
    CdReadyEntry            entry;
    volatile CdStreamState* p;

    CdStream_State.readySlot = 0;

    if ((CdStream_State.flags0 >> 2) & 1) {
        if (CdStream_State.field_1C == 0) {
            CdStream_State.phase  = 1;
            CdStream_State.flags1 = CdStream_State.flags1 | 1;
            CdStream_State.flags2 = CdStream_State.flags2 & 0xFD;
            CdStream_State.flags2 = CdStream_State.flags2 & 0xFB;
            if (CdStream_State.doneCb != NULL) {
                CdStream_State.doneCb(1);
            }
            return;
        }
    }

    p               = &CdStream_State;
    entry.pollFn    = (s32)func_80059EE0;
    entry.doneFn    = (s32)CdStream_Continue;
    p->flags2       = p->flags2 & 0xFD;
    entry.errorFn   = (s32)CdStream_FinishQueueEntry;
    entry.sectorPos = p->startSector;
    D_80068B63      = D_80068B63 + 1;
    p->readySlot    = CdReady_Enqueue(&entry);
}

void CdStream_Stop(void)
{
    u8                      saved;
    u8                      temp;
    s16                     arg0;
    s16                     idx;
    u32                     flags;
    CdReadyEntry*           entry;
    volatile CdStreamState* p;

    saved                = CdReady_Queue.locked;
    CdReady_Queue.locked = 1;

    if (CdStream_State.flags0 & 1) {
        CdStream_State.flags0 = CdStream_State.flags0 | 0x40;
        SpuSetIRQ(0);
        SpuSetIRQCallback(0);
    } else {
        if (CdStream_State.readySlot != 0) {
            arg0 = CdStream_State.readySlot;
            temp = CdReady_Queue.locked;
            if (arg0 != 0) {
                idx   = arg0 - 1;
                entry = (CdReadyEntry*)&CdReady_Queue.entries[idx];
                flags = entry->flags;
                if (flags & 1) {
                    entry->flags = (flags & ~1) | 4;
                }
                CdReady_Queue.locked = temp;
            }
            CdStream_State.readySlot = 0;
        }

        p = &CdStream_State;
        if ((p->flags2 >> 3) & 1) {
            func_800B0118(0, 0);
            p->flags2 = p->flags2 & 0xF7;
        }
    }

    CdReady_Queue.locked = saved;
}

void func_80058748(void);

void CdStream_TeardownVoices(void)
{
    CdReadyEntry            entry;
    volatile CdStreamState* p;
    s32                     flag;
    s16                     arg0;
    s16                     idx;
    u32                     flags;
    CdReadyEntry*           e;
    u8                      saved;
    u8                      t;
    s32                     dead;
    register s32            rem_tmp asm("a0");
    register s32            temp asm("v0");

    p = &CdStream_State;
    if (p->flags2 & 1) {
        dead                  = p->field_20;
        t                     = p->flags2;
        t                     = t & 0xFE;
        p->flags2             = t;
        t                     = CdStream_State.flags0;
        t                     = t & 0xFE;
        CdStream_State.flags0 = t;
        p->flags2             = p->flags2 & 0xFD;
        CdStream_State.flags0 = CdStream_State.flags0 & 0xDF;
        flag                  = (CdStream_State.flags0 >> 4) & 1;
        if (flag == 1) {
            Spu_KeyOff((s8)p->voiceL);
            Spu_KeyOff((s8)p->voiceR);
            CdStream_State.flags0 = CdStream_State.flags0 & 0xEF;
            if (p->voiceFreeCb != NULL) {
                p->voiceFreeCb((flag << (s8)p->voiceL) | (flag << (s8)p->voiceR));
            }
        }
        SpuSetIRQ(0);
        SpuSetIRQCallback(0);
        if (CdStream_State.readySlot != 0) {
            arg0  = CdStream_State.readySlot;
            saved = CdReady_Queue.locked;
            if (arg0 != 0) {
                idx   = arg0 - 1;
                e     = (CdReadyEntry*)&CdReady_Queue.entries[idx];
                flags = e->flags;
                if (flags & 1) {
                    e->flags = (flags & ~1) | 4;
                }
                CdReady_Queue.locked = saved;
            }
            CdStream_State.readySlot = 0;
        }
        rem_tmp = (s32)&entry;
        asm("" : "+r"(rem_tmp));
        do {
            temp = (s32)func_80059EE0;
        } while (0);
        entry.pollFn          = temp;
        temp                  = (s32)func_80058748;
        p                     = &CdStream_State;
        entry.doneFn          = temp;
        entry.sectorPos       = p->field_30;
        CdStream_State.flags0 = CdStream_State.flags0 & 0xFB;
        entry.errorFn         = (s32)CdStream_FinishQueueEntry;
        p->readySlot          = CdReady_Enqueue((CdReadyEntry*)rem_tmp);
        p->phase              = 2;
    }
}

void func_80058748(void)
{
    CdReadyEntry            entry;
    volatile CdStreamState* p;
    register s32            rem_tmp asm("a0");
    register s32            field18 asm("a1");
    register s32            temp asm("v0");
    register s32            rem asm("v1");
    register s32            quot asm("a3");

    p            = &CdStream_State;
    field18      = p->field_18;
    temp         = field18 / p->sectorsPerChunk;
    p->readySlot = 0;
    quot         = temp + 1;

    if ((CdStream_State.flags0 >> 2) & 1) {
        p->phase  = 1;
        p->flags1 = p->flags1 | 1;
        p->flags2 = p->flags2 & 0xFD;
        if (p->field_1C & 1) {
            p->field_4 = 0;
        } else {
            rem_tmp               = (u16)p->sectorsPerChunk;
            rem                   = field18 % p->sectorsPerChunk;
            rem_tmp               = rem_tmp - rem;
            rem_tmp               = rem_tmp + 1;
            p->field_4            = rem_tmp;
            CdStream_State.flags0 = CdStream_State.flags0 | 0x20;
        }
        CdStream_CleanupIrq();
    } else {
        do {
            temp = (s32)func_80059EE0;
        } while (0);
        rem             = p->flags2;
        entry.pollFn    = temp;
        temp            = (s32)func_80058748;
        rem             = rem & 0xFD;
        p->flags2       = rem;
        entry.doneFn    = temp;
        D_80068B63      = D_80068B63 + 1;
        temp            = (s32)CdStream_FinishQueueEntry;
        rem             = p->field_30;
        entry.errorFn   = temp;
        p->field_20     = quot;
        rem_tmp         = (s32)&entry;
        entry.sectorPos = rem;
        p->readySlot    = CdReady_Enqueue((CdReadyEntry*)rem_tmp);
    }
}

void CdStream_CleanupIrq(void)
{
    volatile CdStreamState* p;
    u8                      temp;

    if (D_80068B5C != 0) {
        SpuSetIRQ(0);
        SpuSetIRQCallback(0);
        D_80068B5C = 0;
    }
    CdStream_State.flags0 = CdStream_State.flags0 & 0xF7;
    CdStream_State.flags0 = CdStream_State.flags0 & 0xBF;
    func_800B0118(0, 0);
    p                     = &CdStream_State;
    temp                  = p->flags2;
    p->flags2             = temp & 0xF7;
    D_80082808            = 0;
    CdStream_State.flags0 = CdStream_State.flags0 | 1;
}

INCLUDE_ASM("main/nonmatchings/cdaudio", func_8005896C);

INCLUDE_ASM("main/nonmatchings/cdaudio", func_80058ED4);

INCLUDE_ASM("main/nonmatchings/cdaudio", CdStream_Drive);
