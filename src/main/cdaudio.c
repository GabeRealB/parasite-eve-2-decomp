#include "cdaudio.h"

#include <psyq/sys/types.h>
#include <psyq/libcd.h>
#include <psyq/libspu.h>

#include "common.h"

#include "main/cdaudio_types.h"
#include "cdstream.h"
#include "main/fs.h"
#include "fs.h"
#include "fs_types.h"
#include "main/sound.h"
#include "sound.h"
#include "main/sound_types.h"
#include "sound_types.h"

/// `_CdAudioPlayback::driver` values: the `CdAudio_DriveFns` entry `CdAudio_Tick`
/// runs. Each driver returns the value to run on the next tick.
enum {
    CD_AUDIO_DRIVER_IDLE        = 0, // nothing to do
    CD_AUDIO_DRIVER_PLAY        = 1, // start the stream at `startSector`, then leave it playing
    CD_AUDIO_DRIVER_STOP        = 2, // arm the stream's SPU interrupt, wait for it to go idle, then release its voices
    CD_AUDIO_DRIVER_FADE_OUT    = 3, // fade the stream down, stop it and release its voices
    CD_AUDIO_DRIVER_READ_HEADER = 5, // read the header sector at `baseSector`, then wait for a track to be chosen
    CD_AUDIO_DRIVER_LOAD_WAVES  = 6, // upload the wave data starting at `baseSector` to the SPU
};

/// Bits a 0..127 track level is shifted up by to give `_CdAudioPlayback::volume`.
#define CD_AUDIO_VOLUME_LEVEL_SHIFT 7

/// What the CD audio player is working on and which of its drivers is running.
///
/// `baseSector` is where the player's own reads start: the header sector whose
/// entries place each track as a sector offset from it, or the first sector of
/// a wave upload. A track started directly names its sector and reads no header.
typedef struct {
    u8  driver;        // `CD_AUDIO_DRIVER_*` run each tick
    u8  startReported; // raised when the stream reports its start as finished or abandoned
    u16 volume;        // stream gain in SPU volume units: a 0..127 level << `CD_AUDIO_VOLUME_LEVEL_SHIFT`
    s32 baseSector;    // absolute disc sector the player reads from; no driver runs while it is 0
    s32 spuBase;       // SPU address of the stream's left ring
    s32 startSector;   // absolute disc sector the stream starts from
} _CdAudioPlayback;
STATIC_ASSERT_SIZEOF(_CdAudioPlayback, 0x10);

/// BSS object CdAudio_Tbl (size 0x18). CD/audio stream state for 46FE4.c.
/// field_C is a base pointer into a halfword table; CdAudio_LoadSectorEntry indexes it
/// with ((packed >> 14) & 0x3FC) / 2 (4-byte stride, low halfword of each slot).
typedef struct _CdAudioTbl {
    /* 0x00 */ u8   field_0;
    /* 0x01 */ u8   field_1;
    /* 0x02 */ u8   field_2; // index into CdAudio_TblEntries
    /* 0x03 */ u8   pad_3;
    /* 0x04 */ s32  field_4;
    /* 0x08 */ s32  field_8;
    /* 0x0C */ u16* field_C;  // halfword table base (CdAudio_LoadSectorEntry)
    /* 0x10 */ s32  field_10; // transfer / SpuWrite param
    /* 0x14 */ s32  field_14;
} CdAudioTbl;
STATIC_ASSERT_SIZEOF(CdAudioTbl, 0x18);

/// BSS object CdAudio_Ctl (size 0x14). CD stream control for 46FE4.c.
typedef struct _CdAudioCtl {
    /* 0x00 */ s32 field_0;  // busy / retry counter
    /* 0x04 */ s32 field_4;  // secondary counter
    /* 0x08 */ u8  field_8;  // phase mirror
    /* 0x09 */ u8  field_9;
    /* 0x0A */ s8  field_A;  // error code (-1 / -2)
    /* 0x0B */ u8  field_B;
    /* 0x0C */ s32 field_C;  // countdown
    /* 0x10 */ s32 field_10; // control flag
} CdAudioCtl;
STATIC_ASSERT_SIZEOF(CdAudioCtl, 0x14);

/// 4-byte entry pointed to by CdAudio_TblEntries (see CdAudio_PrepareNextEntry).
/// Indexed by CdAudio_Tbl.field_2; field_3 is compared across adjacent entries.
typedef struct _CdAudioTblEntry {
    /* 0x0 */ u8 pad[3];
    /* 0x3 */ u8 field_3; // compared across adjacent entries for span
} CdAudioTblEntry;
STATIC_ASSERT_SIZEOF(CdAudioTblEntry, 0x4);

/// The CD audio player's working state: its playback, the location it seeks
/// to, the ramp that winds the stream down, and the stream setup. CdAudio_Init
/// repeatedly clears only its first word; the original loop never advances
/// its destination pointer.
typedef struct {
    volatile _CdAudioPlayback playback;
    CdlLOC                    setloc; // passed to CdlSetloc to start a seek
    LinInterp                 ramp;   // Fades the stream volume down before a stop
    CdStreamParams            stream; // setup handed to CdStream_Start
} _CdAudioState;

typedef struct {
    /* 0x0 */ u8 pad[2];
    /* 0x2 */ u8 field_2;
    /* 0x3 */ u8 field_3;
} SectorHdr;

/* Define BSS before API headers to preserve first-declaration order. */
static u8* CdAudio_SectorBuffer;

static u8 D_80082754;

static volatile CdAudioTbl CdAudio_Tbl;

static volatile s32 D_80082770;

/// Unreferenced.
static u8 D_80082774[4];

static u32* CdAudio_SectorEntries;

static volatile u8 D_8008277C;

static volatile CdAudioCtl CdAudio_Ctl;

static CdAudioTblEntry* CdAudio_TblEntries;

volatile CdAudioPhase CdAudio_Phase;

static _CdAudioState _gCdAudioState;

static volatile u8 D_800827E4;

#include "main/cdaudio.h"

extern s32 (*CdAudio_DriveFns[])(void);

/// Per-track initial gains in 1/128 units of the SPU volume register.
static u8 CdAudio_VolumeTable[];

static s32 D_80068B18[1];

static s32 D_80068B1C;

/// Unreferenced.
static s32 D_80068B20[2];

static s16 D_80068B28[];

static s32 D_80068B2C[];

static s32 CdAudio_DriveStream(void);

static s32 CdAudio_DrivePhase0(void);

static s32 CdAudio_DriveSeek(void);

static s32 CdAudio_DriveRead(void);

static void CdAudio_FeedSector(u8 arg0, u8* unusedResult);

static u8 CdAudio_GetState(void);

static s32 CdAudio_Reset(s32 arg0);

static s32 CdAudio_SetupStream(void);

static s32 CdAudio_SeekRelative(s32 arg0);

static s32 CdAudio_RequestStopA(void);

static s32 CdAudio_PrepareNextEntry(void);

static s32 CdAudio_ResetKeepBuffer(s32 arg0);

static s32 CdAudio_StoreIfNonNull(s32 arg0);

static void CdAudio_SetLocBase(s32 arg0);

static s32 CdAudio_LoadSectorEntry(s32 arg0);

static s32 CdAudio_SeekAbs(s32 arg0);

static s32 CdAudio_RequestStop(void);

static void CdAudio_StartVolumeRamp(s32 arg0);

static void CdAudio_JumpWithPitch(s32 arg0, s32 arg1);

static s32 CdAudio_DriveNull(void);

static s32 CdAudio_DrivePhase1(void);

static void CdAudio_ReadyCallback(u8 arg0, u8* unusedResult);

static void CdAudio_SetLocFlag(s32 unused);

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
    if (_gCdAudioState.playback.driver == CD_AUDIO_DRIVER_LOAD_WAVES) {
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
    s16                    volume;

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
            CdIntToPos(state->playback.startSector, loc);
            if (D_8008277C != 0) {
                volume = 0;
            } else {
                volume = state->playback.volume;
            }
            setup->voiceR                 = CD_STREAM_VOICE_NONE;
            setup->voiceL                 = CD_STREAM_VOICE_NONE;
            setup->volume                 = volume;
            setup->channelCount           = 2;
            setup->sectorBuf              = &Fs_CdSector;
            setup->spuBase                = state->playback.spuBase;
            setup->startSector            = CdPosToInt(loc);
            setup->doneCb                 = CdAudio_SetLocFlag;
            setup->startCb                = NULL;
            setup->voiceFreeCb            = NULL;
            state->playback.startReported = 0;
            CdStream_Start(setup);
            CdAudio_Phase.field_0 = 2;
            break;
        case 2:
            if (_gCdAudioState.playback.startReported != 0) {
                p = &CdAudio_Phase;
                if (p->field_5 == 1) {
                    p->field_2 = 4;
                }
                CdAudio_Phase.field_0                 = 3;
                _gCdAudioState.playback.startReported = 0;
            }
            break;
        case 0:
        case 3:
            break;
    }
    return CD_AUDIO_DRIVER_PLAY;
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
    ret = CD_AUDIO_DRIVER_FADE_OUT;

    switch (p->field_2) {
        case 1:
            ramp       = &_gCdAudioState.ramp;
            p->field_1 = 4;
            LinInterp_Step(ramp);
            if (ramp->gain == ramp->targetGain) {
                ramp->enabled = LINEAR_INTERPOLATOR_BYPASS;
                CdStream_SetVolume(0);
                CdStream_Stop();
                p->field_2 = 2;
            } else {
                CdStream_SetVolume((s16)LinInterp_Apply(ramp, _gCdAudioState.playback.volume));
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
                ret                   = CD_AUDIO_DRIVER_IDLE;
            }
            break;
    }
    return ret;
}

static u8 CdAudio_VolumeTable[] = {
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
    s32                  tmp;

    phase = CdAudio_Phase.field_3;
    hdr   = (SectorHdr*)CdAudio_SectorBuffer;

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
            if (stream->field_A != 0) {
                stream->field_8 = CdAudio_Phase.field_3;
                stream->field_9 = 2;
                goto error;
            }
            if (stream->field_0 < 0x259) {
                if (D_80082770 != 0) {
                    _gCdAudioState.playback.spuBase = D_80068B18[hdr->field_3];
                    CdAudio_Tbl.field_C             = (u16*)(CdAudio_SectorBuffer + hdr->field_2 * 4);
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
            tmp                 = CdAudio_Ctl.field_0;
            tmp                 = tmp + 1;
            CdAudio_Ctl.field_0 = tmp;
            return CD_AUDIO_DRIVER_READ_HEADER;
    }

    tmp                 = CdAudio_Ctl.field_0;
    tmp                 = tmp + 1;
    CdAudio_Ctl.field_0 = tmp;
    return CD_AUDIO_DRIVER_READ_HEADER;

error:
    CdAudio_Phase.field_3 = 0x80;
    return CD_AUDIO_DRIVER_IDLE;
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
            CdAudio_Tbl.field_8 = _gCdAudioState.playback.baseSector;
            CdIntToPos(_gCdAudioState.playback.baseSector, &_gCdAudioState.setloc);
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
            ret                   = CD_AUDIO_DRIVER_IDLE;
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
    return CD_AUDIO_DRIVER_LOAD_WAVES;
}

static void CdAudio_FeedSector(u8 arg0, u8* unusedResult)
{
    s32                  arg;
    s32                  pos;
    s32                  ret;
    SndLoadState*        state;
    s32                  spuIdx;
    volatile CdAudioCtl* stream;
    volatile CdAudioTbl* cdState;
    FsSector*            sector;

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
    pos = CdPosToInt(&sector->location);
    if (cdState->field_8 != pos) {
        cdState->field_1 = 0xFF;
        stream->field_B  = arg;
        return;
    }
    CdGetSector(sector, 0x200);
    if (_gCdAudioState.playback.baseSector == cdState->field_8) {
        // First waveform sector: skip its lead and upload samples.
        // A busy SPU transfer keeps the bank.
        state                                 = &SndLoad_State;
        state->feedMode                       = SOUND_LOAD_FEED_CD_AUDIO;
        state->payload.header.waveBlockOffset = 0;
        state->sectorsArrived                 = 0;
        state->syncUpload                     = 0;
        state->sectorBytes                    = SOUND_LOAD_WAVE_FIRST_BYTES;
        state->phase                          = SOUND_LOAD_PHASE_UPLOAD_WAVE;
        state->bytesRemaining                 = sector->words[1];
        state->payload.header.transferSectors = sector->bytes[0];
        spuIdx                                = sector->words[2];
        if (spuIdx != 0) {
            SpuSetTransferStartAddr(D_80068B2C[spuIdx]);
        }
        if (SndLoad_ProcessSector(&sector->words[SOUND_LOAD_WAVE_LEAD_BYTES / sizeof(u32)]) == SOUND_LOAD_PHASE_ERROR) {
            stream->field_B = 3;
            return;
        }
        state->sectorBytes = SOUND_LOAD_SECTOR_BYTES;
    } else {
        ret = SndLoad_ProcessSector(sector->words);
        if (ret == SOUND_LOAD_PHASE_DONE) {
            cdState->field_1 = arg;
            CdReadyCallback(0);
        } else if (ret == SOUND_LOAD_PHASE_ERROR) {
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

    D_8008277C                      = 0;
    CdAudio_SectorBuffer            = 0;
    _gCdAudioState.playback.spuBase = 0x51010;
    Spu_SetVoiceRange(3, 0x16, 2);
    CdStream_Reset();
    CdVol_SetMixMode(1);
}

static u8 CdAudio_GetState(void)
{
    return _gCdAudioState.playback.driver;
}

void CdAudio_Tick(void)
{
    if ((_gCdAudioState.playback.baseSector != 0) && (_gCdAudioState.playback.driver != CD_AUDIO_DRIVER_IDLE)) {
        _gCdAudioState.playback.driver = CdAudio_DriveFns[_gCdAudioState.playback.driver & 7]();
        CdStream_Drive();
    }
}

static s32 CdAudio_Reset(s32 arg0)
{
    volatile CdAudioCtl* p;

    D_800827E4                         = 0;
    D_80082754                         = 0;
    p                                  = &CdAudio_Ctl;
    p->field_C                         = 0;
    p->field_8                         = 0;
    p->field_9                         = 0;
    _gCdAudioState.playback.baseSector = arg0;
    CdAudio_SectorBuffer               = 0;
    return 0;
}

static s32 CdAudio_SetupStream(void)
{
    u8             mode;
    u8*            mem;
    u8*            buf;
    _CdAudioState* state;

    CdAudio_Phase.field_3 = 5;
    state                 = &_gCdAudioState;
    CdIntToPos(state->playback.baseSector, &state->setloc);
    buf = CdAudio_SectorBuffer;
    if (buf != 0) {
        SndHeap_Free(buf);
    }
    mem                            = SndHeap_Malloc(0x800);
    CdAudio_SectorBuffer           = mem;
    CdAudio_SectorEntries          = (u32*)(mem + 4);
    _gCdAudioState.playback.driver = CD_AUDIO_DRIVER_READ_HEADER;
    mode                           = CdlModeSpeed | CdlModeSize1;
    CdControlB(CdlSetmode, &mode, NULL);
    return 0;
}

static s32 CdAudio_SeekRelative(s32 arg0)
{
    s32 temp_s0;

    temp_s0 = arg0 & 0xFF;
    if (temp_s0 != 0) {
        CdAudio_SeekAbs(_gCdAudioState.playback.baseSector + CdAudio_LoadSectorEntry((arg0 - 1) & 0xFF));
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

s32 CdAudio_StartTrack(s32 sector, s32 volumeIndex)
{
    if (CdStream_IsBusy() != 0) {
        return -1;
    }
    CdAudio_ResetKeepBuffer(sector);
    _gCdAudioState.playback.volume = CdAudio_VolumeTable[volumeIndex & 0xFF] << CD_AUDIO_VOLUME_LEVEL_SHIFT;
    SndEvt_EnqueueType7(SOUND_BANK_TYPE_ALL_NON_AMBIENT, 0);
    return CdAudio_StoreIfNonNull(sector);
}

static s32 CdAudio_ResetKeepBuffer(s32 arg0)
{
    volatile CdAudioCtl*   p;
    volatile CdAudioPhase* q;
    s32                    baseSector;

    D_800827E4 = 0;
    baseSector = _gCdAudioState.playback.baseSector;
    D_80082754 = 0;
    if (baseSector == 0) {
        _gCdAudioState.playback.baseSector = arg0;
    }
    p                               = &CdAudio_Ctl;
    p->field_C                      = 0;
    p->field_8                      = 0;
    p->field_9                      = 0;
    p->field_10                     = 1;
    _gCdAudioState.playback.spuBase = D_80068B1C;
    q                               = &CdAudio_Phase;
    q->field_2                      = 0;
    q->field_0                      = 0;
    q->field_1                      = 0;
    q->field_4                      = 0;
    q->field_5                      = 0;
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
    _gCdAudioState.playback.baseSector = arg0;
}

void CdAudio_CopyVoiceData(s8 arg0, const SpuVoiceAttr* attr)
{
    SpuVoiceRef sp10;
    s32*        dest;
    const s32*  arg1;
    u32         i;

    Spu_SetVoiceCallbacks(arg0, NULL, NULL);
    Spu_GetVoiceRef(arg0, &sp10);
    dest = (s32*)sp10.field_4;
    arg1 = (const s32*)attr;
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

    temp_v0                        = CdAudio_SectorEntries[D_80082754 + (arg0 & 0xFF)];
    _gCdAudioState.playback.volume = (temp_v0 >> 17) & 0x3F80;
    CdAudio_Tbl.field_0            = temp_v0 >> 31;
    table                          = CdAudio_Tbl.field_C;
    CdAudio_Tbl.field_4            = table[((temp_v0 >> 14) & 0x3FC) / 2];
    return temp_v0 & 0xFFFF;
}

static s32 CdAudio_SeekAbs(s32 arg0)
{
    s32 temp_v0;

    temp_v0 = CdAudio_Phase.field_1;
    if ((temp_v0 == 4) || (temp_v0 == 0)) {
        _gCdAudioState.playback.startSector = arg0;
        _gCdAudioState.playback.driver      = CD_AUDIO_DRIVER_PLAY;
        CdAudio_Phase.field_0               = 4;
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
        ret                            = 0;
        p->field_2                     = 0;
        p->field_1                     = 1;
        _gCdAudioState.playback.driver = CD_AUDIO_DRIVER_STOP;
    }
    return ret;
}

static void CdAudio_StartVolumeRamp(s32 arg0)
{
    LinInterp* ramp;

    ramp = &_gCdAudioState.ramp;
    LinInterp_Setup(ramp, (_gCdAudioState.playback.volume >> CD_AUDIO_VOLUME_LEVEL_SHIFT) & 0xFF, 0, arg0);
    CdAudio_Phase.field_1          = 4;
    CdAudio_Phase.field_2          = 1;
    _gCdAudioState.playback.driver = CD_AUDIO_DRIVER_FADE_OUT;
}

static void CdAudio_JumpWithPitch(s32 arg0, s32 arg1)
{
    _gCdAudioState.playback.baseSector = arg0;
    CdAudio_Tbl.field_10               = arg1;
    CdAudio_Phase.field_4              = 1;
    _gCdAudioState.playback.driver     = CD_AUDIO_DRIVER_LOAD_WAVES;
}

static s32 CdAudio_DriveNull(void)
{
    return CD_AUDIO_DRIVER_IDLE;
}

static s32 CdAudio_DrivePhase1(void)
{
    s16 ret;

    ret = CD_AUDIO_DRIVER_STOP;
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
                ret                   = CD_AUDIO_DRIVER_FADE_OUT;
                CdAudio_Phase.field_1 = 4;
                CdAudio_Phase.field_2 = 2;
            }
            break;
        case 4:
            break;
    }
    return ret;
}

static void CdAudio_ReadyCallback(u8 arg0, u8* unusedResult)
{
    s32                        temp;
    s32                        pos;
    FsSector*                  sector;
    volatile _CdAudioPlayback* playback;

    if (D_80082770 != 0) {
        return;
    }

    temp = arg0 & 0xFF;
    if (temp == 1) {
        sector = &Fs_CdSector;
        CdGetSector(sector, 3);
        playback = &_gCdAudioState.playback;
        pos      = CdPosToInt(&sector->location);
        if (playback->baseSector != pos) {
            CdAudio_Ctl.field_A = -2;
        }
        CdGetSector(CdAudio_SectorBuffer, 0x200);
        D_80082770 = temp;
    } else {
        CdAudio_Ctl.field_A = -1;
    }
}

static void CdAudio_SetLocFlag(s32 unused)
{
    _gCdAudioState.playback.startReported = 1;
}
