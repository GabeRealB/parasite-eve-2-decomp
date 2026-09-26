#include "common.h"

#define CDAUDIO_C

#include "main/unknown_syms.h"
#include "main/cdaudio.h"
#include "main/cdstream.h"
#include "main/fs.h"

/// The CD audio player's working state: its position, the location it seeks
/// to, the ramp that winds the stream down, and the stream setup. CdAudio_Init
/// clears all of it as one block.
typedef struct {
    volatile CdAudioLoc loc;
    CdlLOC              setloc; // passed to CdlSetloc to start a seek
    LinInterp           ramp;   // scales the stream pitch down before a stop
    CdStreamParams      stream; // setup handed to CdStream_Start
} _CdAudioState;

static volatile s32        D_80082750;
static u8                  D_80082754;
static volatile CdAudioTbl CdAudio_Tbl;
static volatile s32        D_80082770;
/// Unreferenced.
static u8                  D_80082774[4];
static s32                 D_80082778;
static volatile u8         D_8008277C;
static volatile CdAudioCtl CdAudio_Ctl;
static CdAudioTblEntry*    CdAudio_TblEntries;
volatile CdAudioPhase      CdAudio_Phase;
static _CdAudioState       _gCdAudioState;
static volatile u8         D_800827E4;

static void CdAudio_JumpWithPitch(s32 arg0, s32 arg1);
static s32  CdAudio_LoadSectorEntry(s32 arg0);
static s32  CdAudio_RequestStop(void);
static s32  CdAudio_ResetKeepBuffer(s32 arg0);
static s32  CdAudio_SeekAbs(s32 arg0);
static void CdAudio_SetLocFlag(void);
static s32  CdAudio_StoreIfNonNull(s32 arg0);

static s32 CdAudio_DriveNull(void);

static void CdAudio_StartVolumeRamp(s32 arg0);

s32 CdAudio_Begin(void)
{
    volatile CdAudioPhase* p;

    p          = &CdAudio_Phase;
    p->field_5 = 1;
    if (CdAudio_Ctl.field_10 == 1) {
        CdAudio_Ctl.field_10 = 0;
        return -3;
    }
    if ((p->field_1 == 0) || (p->field_1 == 4)) {
        p->field_2 = 4;
        return -2;
    }
    if (p->field_0 != 3) {
        p->field_5 = 1;
        return -1;
    }
    if (_gCdAudioState.loc.field_0 == 6) {
        if (p->field_4 != 0) {
            p->field_4 = 0xB;
            return -2;
        }
    }
    if (CdAudio_Phase.field_2 != 0) {
        return 1;
    }
    CdAudio_StartVolumeRamp(0x20);
    return 0;
}

static s32 CdAudio_DriveStream(void)
{
    volatile CdAudioPhase* p;
    _CdAudioState*         state;
    CdStreamParams*        setup;
    CdlLOC*                loc;
    s8                     i;
    s8                     status;
    s16                    half;

    setup = &_gCdAudioState.stream;
    switch (CdAudio_Phase.field_0) {
        case 4:
            CdAudio_Ctl.field_10 = 0;
            status               = 0;
            for (i = 0x16; i < 0x18; i++) {
                status += Spu_GetVoiceStatus(i);
                if (status != 0) {
                    Spu_KeyOff(i);
                }
            }
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
            state = &_gCdAudioState;
            loc   = &state->setloc;
            CdIntToPos(state->loc.field_C, loc);
            if (D_8008277C != 0) {
                half = 0;
            } else {
                half = state->loc.field_2;
            }
            setup->voiceR      = -1;
            setup->voiceL      = -1;
            setup->pitch       = half;
            setup->mode        = 2;
            setup->sectorBuf   = &Fs_CdSector;
            setup->spuBase     = state->loc.field_8;
            setup->startSector = CdPosToInt(loc);
            setup->doneCb      = CdAudio_SetLocFlag;
            setup->startCb     = 0;
            setup->voiceFreeCb = 0;
            state->loc.field_1 = 0;
            CdStream_Start(setup);
            CdAudio_Phase.field_0 = 2;
            break;
        case 2:
            if (_gCdAudioState.loc.field_1 != 0) {
                p = &CdAudio_Phase;
                if (p->field_5 == 1) {
                    p->field_2 = 4;
                }
                CdAudio_Phase.field_0      = 3;
                _gCdAudioState.loc.field_1 = 0;
            }
            break;
        case 0:
        case 3:
            break;
    }
    return 1;
}

/* Alignment pad after CdAudio_DriveStream's 5-entry jump table so CdAudio_DriveSeek's
 * compiler-generated jtbl lands at 0x800141DC. */

static s32 CdAudio_DrivePhase0(void)
{
    volatile CdAudioPhase*  p;
    s16                     ret;
    LinInterp*              ramp;
    volatile _CdAudioState* state;

    p   = &CdAudio_Phase;
    ret = 3;

    switch (p->field_2) {
        case 1:
            ramp       = &_gCdAudioState.ramp;
            p->field_1 = 4;
            LinInterp_Step(ramp);
            if (ramp->field_0 == ramp->field_4) {
                ramp->field_E = 0;
                CdStream_SetPitch(0);
                CdStream_Stop();
                p->field_2 = 2;
            } else {
                CdStream_SetPitch((s16)LinInterp_Apply(ramp, _gCdAudioState.loc.field_2));
            }
            break;
        case 2:
            state = &_gCdAudioState;
            Spu_KeyOff(state->stream.voiceL);
            Spu_KeyOff(state->stream.voiceR);
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

static void CdAudio_ReadyCallback(s32 arg0);
static void CdAudio_FeedSector(s32 arg0);
static s32  CdAudio_DrivePhase1(void);
static s32  CdAudio_DriveSeek(void);
static s32  CdAudio_DriveRead(void);

static u8 D_80068A80[] = {
    0x64,
    0x64,
    0x64,
    0x64,
    0x0,
    0x0,
    0x0,
    0x0,
    0x0,
    0x0,
    0x0,
    0x0,
    0x0,
    0x0,
    0x0,
    0x0,
    0x7C,
    0x7F,
    0x78,
    0x7C,
    0x72,
    0x7C,
    0x7E,
    0x6E,
    0x52,
    0x0,
    0x0,
    0x0,
    0x0,
    0x0,
    0x0,
    0x0,
    0x78,
    0x6E,
    0x6F,
    0x76,
    0x62,
    0x6E,
    0x0,
    0x0,
    0x0,
    0x0,
    0x0,
    0x0,
    0x0,
    0x0,
    0x0,
    0x0,
    0x75,
    0x64,
    0x73,
    0x78,
    0x78,
    0x78,
    0x78,
    0x0,
    0x5F,
    0x0,
    0x0,
    0x0,
    0x0,
    0x0,
    0x0,
    0x0,
    0x64,
    0x78,
    0x7B,
    0x7D,
    0x75,
    0x7E,
    0x0,
    0x7E,
    0x0,
    0x0,
    0x0,
    0x0,
    0x0,
    0x0,
    0x0,
    0x0,
    0x64,
    0x7E,
    0x7B,
    0x7F,
    0x0,
    0x0,
    0x0,
    0x0,
    0x0,
    0x0,
    0x0,
    0x0,
    0x0,
    0x0,
    0x0,
    0x0,
    0x69,
    0x50,
    0x7B,
    0x7D,
    0x7F,
    0x7F,
    0x7D,
    0x7A,
    0x66,
    0x61,
    0x78,
    0x0,
    0x0,
    0x0,
    0x0,
    0x0,
};
u8 D_80068AF0[] = {
    0x0,
    0x5A,
    0x5A,
    0x7F,
    0x7F,
    0x7F,
    0x7F,
    0x7F,
    0x7F,
    0x7F,
    0x76,
    0x7F,
    0x7E,
    0x7F,
    0x7F,
    0x7F,
    0x7F,
    0x73,
    0x7F,
    0x7F,
    0x73,
    0x6E,
    0x54,
    0x54,
    0x19,
    0x19,
    0x19,
    0x19,
    0x19,
    0x78,
    0x6E,
    0x5F,
    0x73,
    0x5F,
    0x5F,
    0x7F,
    0x7F,
    0x73,
    0x73,
    0x70,
};
static s32 D_80068B18[1] = { 0x51010 };
static s32 D_80068B1C    = 0x51010;
/// Unreferenced.
static s32 D_80068B20[2]               = { 0x51010, 0x51010 };
static s16 D_80068B28[]                = { 3, 1 };
static s32 D_80068B2C[]                = { 0x63810, 0x63810 };
s32        (*CdAudio_DriveFns[])(void) = {
    CdAudio_DriveNull,
    CdAudio_DriveStream,
    CdAudio_DrivePhase1,
    CdAudio_DrivePhase0,
    CdAudio_DriveNull,
    CdAudio_DriveSeek,
    CdAudio_DriveRead,
    CdAudio_DriveNull,
};

static s32 CdAudio_DriveSeek(void)
{
    u8                   phase;
    SectorHdr*           hdr;
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
            CdControlF(CdlSetloc, (u8*)&_gCdAudioState.setloc);
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
                    audio          = &_gCdAudioState.loc;
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

static s32 CdAudio_DriveRead(void)
{
    volatile _CdAudioState* state;
    volatile CdAudioCtl*    stream;
    volatile CdAudioCtl*    p;
    volatile CdAudioTbl*    cd;
    s32                     status;
    u8                      mode;
    s32                     ret;

    switch (CdAudio_Phase.field_4) {
        case 1:
            state = &_gCdAudioState;
            Spu_KeyOff(state->stream.voiceL);
            Spu_KeyOff(state->stream.voiceR);
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
            CdAudio_Tbl.field_8 = _gCdAudioState.loc.field_4;
            CdIntToPos(_gCdAudioState.loc.field_4, &_gCdAudioState.setloc);
            CdAudio_Phase.field_4 = 5;
            CdControlF(CdlSetloc, (u8*)&_gCdAudioState.setloc);
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

static void CdAudio_FeedSector(s32 arg0)
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
    audio = &_gCdAudioState.loc;
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

    p = (s32*)&_gCdAudioState;
    i = 0;
    do {
        i++;
        *p = 0;
    } while (i < 0x11U);

    D_8008277C                 = 0;
    D_80082750                 = 0;
    _gCdAudioState.loc.field_8 = 0x51010;
    Spu_SetVoiceRange(3, 0x16, 2);
    CdStream_Reset();
    CdVol_SetMixMode(1);
}

static u8 CdAudio_GetState(void)
{
    return _gCdAudioState.loc.field_0;
}

void CdAudio_Tick(void)
{
    if ((_gCdAudioState.loc.field_4 != 0) && (_gCdAudioState.loc.field_0 != 0)) {
        _gCdAudioState.loc.field_0 = CdAudio_DriveFns[_gCdAudioState.loc.field_0 & 7]();
        CdStream_Drive();
    }
}

static s32 CdAudio_Reset(s32 arg0)
{
    volatile CdAudioCtl* p;

    D_800827E4                 = 0;
    D_80082754                 = 0;
    p                          = &CdAudio_Ctl;
    p->field_C                 = 0;
    p->field_8                 = 0;
    p->field_9                 = 0;
    _gCdAudioState.loc.field_4 = arg0;
    D_80082750                 = 0;
    return 0;
}

static s32 CdAudio_SetupStream(void)
{
    u8             mode;
    s32            mem;
    s32            buf;
    _CdAudioState* state;

    CdAudio_Phase.field_3 = 5;
    state                 = &_gCdAudioState;
    CdIntToPos(state->loc.field_4, &state->setloc);
    buf = D_80082750;
    if (buf != 0) {
        SndHeap_Free((void*)buf);
    }
    mem                        = (s32)SndHeap_Malloc(0x800);
    D_80082750                 = mem;
    D_80082778                 = mem + 4;
    _gCdAudioState.loc.field_0 = 5;
    mode                       = CdlModeSpeed | CdlModeSize1;
    CdControlB(CdlSetmode, &mode, NULL);
    return 0;
}

static s32 CdAudio_SeekRelative(s32 arg0)
{
    s32 temp_s0;

    temp_s0 = arg0 & 0xFF;
    if (temp_s0 != 0) {
        CdAudio_SeekAbs(_gCdAudioState.loc.field_4 + CdAudio_LoadSectorEntry((arg0 - 1) & 0xFF));
    }
    return temp_s0;
}

static s32 CdAudio_RequestStopA(void)
{
    return CdAudio_RequestStop();
}

static s32 CdAudio_PrepareNextEntry(void)
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

s32 CdAudio_StartTrack(s32 arg0, s32 arg1)
{
    if (CdStream_IsBusy() != 0) {
        return -1;
    }
    CdAudio_ResetKeepBuffer(arg0);
    _gCdAudioState.loc.field_2 = D_80068A80[arg1 & 0xFF] << 7;
    SndEvt_EnqueueType7(0x80000000, 0);
    return CdAudio_StoreIfNonNull(arg0);
}

static s32 CdAudio_ResetKeepBuffer(s32 arg0)
{
    volatile CdAudioCtl*   p;
    volatile CdAudioLoc*   r;
    volatile CdAudioPhase* q;
    s32                    field4;

    D_800827E4 = 0;
    r          = &_gCdAudioState.loc;
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

static s32 CdAudio_StoreIfNonNull(s32 arg0)
{
    if (arg0 != 0) {
        CdAudio_SeekAbs(arg0);
    }
    return arg0;
}

s32 CdAudio_RequestStopB(void)
{
    return CdAudio_RequestStop();
}

s32 CdAudio_JumpToSector(s32 arg0)
{
    if (arg0 == 0) {
        return -1;
    }
    CdAudio_JumpWithPitch(arg0, D_80082124);
    return 0;
}

static void CdAudio_SetLocBase(s32 arg0)
{
    _gCdAudioState.loc.field_4 = arg0;
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
    Spu_DisableReverbVoice(*arg0);
    Spu_DisableReverbVoice(*arg1);
}

static s32 CdAudio_LoadSectorEntry(s32 arg0)
{
    u32  temp_v0;
    u16* table;

    temp_v0                    = ((u32*)D_80082778)[D_80082754 + (arg0 & 0xFF)];
    _gCdAudioState.loc.field_2 = (temp_v0 >> 17) & 0x3F80;
    CdAudio_Tbl.field_0        = temp_v0 >> 31;
    table                      = CdAudio_Tbl.field_C;
    CdAudio_Tbl.field_4        = table[((temp_v0 >> 14) & 0x3FC) / 2];
    return temp_v0 & 0xFFFF;
}

static s32 CdAudio_SeekAbs(s32 arg0)
{
    s32 temp_v0;

    temp_v0 = CdAudio_Phase.field_1;
    if ((temp_v0 == 4) || (temp_v0 == 0)) {
        _gCdAudioState.loc.field_C = arg0;
        _gCdAudioState.loc.field_0 = 1;
        CdAudio_Phase.field_0      = 4;
    }
    return 0;
}

static s32 CdAudio_RequestStop(void)
{
    volatile CdAudioPhase* p;
    s32                    ret;

    p = &CdAudio_Phase;
    if (p->field_0 != 3) {
        ret        = -1;
        p->field_1 = 4;
        p->field_2 = 1;
    } else {
        ret                        = 0;
        p->field_2                 = 0;
        p->field_1                 = 1;
        _gCdAudioState.loc.field_0 = 2;
    }
    return ret;
}

static void CdAudio_StartVolumeRamp(s32 arg0)
{
    LinInterp* ramp;

    ramp = &_gCdAudioState.ramp;
    LinInterp_Setup(ramp, (_gCdAudioState.loc.field_2 >> 7) & 0xFF, 0, arg0);
    CdAudio_Phase.field_1      = 4;
    CdAudio_Phase.field_2      = 1;
    _gCdAudioState.loc.field_0 = 3;
}

static void CdAudio_JumpWithPitch(s32 arg0, s32 arg1)
{
    _gCdAudioState.loc.field_4 = arg0;
    CdAudio_Tbl.field_10       = arg1;
    CdAudio_Phase.field_4      = 1;
    _gCdAudioState.loc.field_0 = 6;
}

static s32 CdAudio_DriveNull(void)
{
    return 0;
}

static s32 CdAudio_DrivePhase1(void)
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

static void CdAudio_ReadyCallback(s32 arg0)
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
        state = &_gCdAudioState.loc;
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

static void CdAudio_SetLocFlag(void)
{
    _gCdAudioState.loc.field_1 = 1;
}
