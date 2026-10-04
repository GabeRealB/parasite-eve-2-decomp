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

/// `CdAudioProgress::headerReadStep` values: reading the header sector at the
/// player's base sector into the sector buffer.
///
/// The step is 0 until the first read is set up; nothing tests for that.
enum {
    CD_AUDIO_HEADER_READ_STEP_WAIT_SET_LOCATION = 1,    // wait for the drive to take the location
    CD_AUDIO_HEADER_READ_STEP_WAIT_READ         = 3,    // wait for the drive to accept the read
    CD_AUDIO_HEADER_READ_STEP_DONE              = 4,    // the header is in the sector buffer and the drive paused
    CD_AUDIO_HEADER_READ_STEP_SET_LOCATION      = 5,    // entry step: point the drive at the header sector
    CD_AUDIO_HEADER_READ_STEP_START_READ        = 6,    // start reading, with the callback that takes the sector
    CD_AUDIO_HEADER_READ_STEP_WAIT_SECTOR       = 8,    // wait for the callback to deliver the sector
    CD_AUDIO_HEADER_READ_STEP_PAUSE             = 9,    // pause the drive
    CD_AUDIO_HEADER_READ_STEP_WAIT_PAUSE        = 10,   // wait for the drive to pause
    CD_AUDIO_HEADER_READ_STEP_FAILED            = 0x80, // a step timed out or the sector arrived wrong; the read is given up
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

/// Ticks a header read or wave load may spend in one wait before the step is
/// given up.
#define CD_AUDIO_WAIT_TIMEOUT_TICKS 601

/// Ticks a wave load lets pass after the drive has taken its mode.
#define CD_AUDIO_WAVE_LOAD_SETTLE_TICKS 5

/// `_CdAudioDriverStatus::failureKind` values: why a header read or wave load
/// gave a step up.
enum {
    CD_AUDIO_FAILURE_NONE    = 0, // nothing has failed since the track was set up
    CD_AUDIO_FAILURE_TIMEOUT = 1, // the wait reached `CD_AUDIO_WAIT_TIMEOUT_TICKS`
    CD_AUDIO_FAILURE_SECTOR  = 2, // the sector callback reported an error
};

/// `_CdAudioDriverStatus::headerReadError` values, raised by the header read's
/// sector callback.
enum {
    CD_AUDIO_HEADER_READ_ERROR_NONE         = 0,  // no error since the read was started
    CD_AUDIO_HEADER_READ_ERROR_NOT_READY    = -1, // the drive interrupted with something other than a ready sector
    CD_AUDIO_HEADER_READ_ERROR_WRONG_SECTOR = -2, // the sector delivered is not the header sector; it is taken all the same
};

/// `_CdAudioDriverStatus::waveLoadError` values, raised by the wave load's
/// sector callback. Once one is set the callback ignores further sectors.
enum {
    CD_AUDIO_WAVE_LOAD_ERROR_NONE         = 0, // no error since the read was started
    CD_AUDIO_WAVE_LOAD_ERROR_WRONG_SECTOR = 1, // the sector delivered is not the next one of the load
    CD_AUDIO_WAVE_LOAD_ERROR_NOT_READY    = 2, // the drive interrupted with something other than a ready sector
    CD_AUDIO_WAVE_LOAD_ERROR_FIRST_SECTOR = 3, // the upload refused the load's first sector
    CD_AUDIO_WAVE_LOAD_ERROR_UPLOAD       = 4, // the upload failed on a later sector
};

/// What the CD audio player's drivers keep beside their steps: how long the
/// header read and the wave load have been waiting on the drive, what their
/// sector callbacks reported, and a record of the last step either gave up.
///
/// Both read drivers count `waitTicks` up once a tick and zero it with each
/// drive command, so it measures the wait that command started. A header read
/// that fails stops the player; a wave load that fails starts over from the
/// drive mode.
///
/// The player's one instance is volatile: the error fields are written by the
/// CD ready callbacks.
typedef struct {
    s32 waitTicks;       // ticks since the read driver last issued a drive command; a wait ends at `CD_AUDIO_WAIT_TIMEOUT_TICKS`
    s32 readTicks;       // ticks the wave load has waited for its upload since the drive accepted the read; zeroed but not counted by the header read
    u8  failedStep;      // header read or wave load step that was last given up; recorded only, nothing reads it
    u8  failureKind;     // `CD_AUDIO_FAILURE_*` for that step; recorded only, nothing reads it
    s8  headerReadError; // `CD_AUDIO_HEADER_READ_ERROR_*`
    u8  waveLoadError;   // `CD_AUDIO_WAVE_LOAD_ERROR_*`
    s32 settleTicks;     // ticks left of the wave load's pause after setting the drive mode
    s32 openPending;     // 1 from a track's set-up until its open driver first runs or a cancel request finds it
} _CdAudioDriverStatus;
STATIC_ASSERT_SIZEOF(_CdAudioDriverStatus, 0x14);

/// 4-byte entry pointed to by CdAudio_TblEntries (see CdAudio_PrepareNextEntry).
/// Indexed by CdAudio_Tbl.field_2; field_3 is compared across adjacent entries.
typedef struct _CdAudioTblEntry {
    /* 0x0 */ u8 pad[3];
    /* 0x3 */ u8 field_3; // compared across adjacent entries for span
} CdAudioTblEntry;
STATIC_ASSERT_SIZEOF(CdAudioTblEntry, 0x4);

/// Everything the CD audio player keeps between ticks: what it is playing,
/// and the three blocks it fills in for the CD drive, the volume ramp and the
/// CD stream.
///
/// The player's one instance is volatile, like its sibling state blocks:
/// `playback` is shared with the CD ready and stream callbacks. `seekLoc`,
/// `ramp` and `streamParams` are handed to code outside the player through
/// plain pointers.
///
/// Initialisation zeroes only the first word, `playback`'s driver, start
/// report and volume: its clear loop stores to that word once per word of the
/// object and never advances.
typedef struct {
    _CdAudioPlayback playback;     // what is playing and which driver runs
    CdlLOC           seekLoc;      // MSF position of the sector being sought: the `CdlSetloc` argument for a header or wave read, or a stream's start
    LinInterp        ramp;         // winds the stream's volume down to silence before a stop
    CdStreamParams   streamParams; // describes the stream to start; both voices are `CD_STREAM_VOICE_NONE` from the first start on
} _CdAudioState;
STATIC_ASSERT_SIZEOF(_CdAudioState, 0x44);

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

static volatile _CdAudioDriverStatus CdAudio_Ctl;

static CdAudioTblEntry* CdAudio_TblEntries;

volatile CdAudioProgress CdAudio_Phase;

static volatile _CdAudioState _gCdAudioState;

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
    volatile CdAudioProgress* progress;

    progress                  = &CdAudio_Phase;
    progress->cancelRequested = 1;
    if (CdAudio_Ctl.openPending == 1) {
        CdAudio_Ctl.openPending = 0;
        return -3;
    }
    if ((progress->playStep == CD_AUDIO_PLAY_STEP_NONE) || (progress->playStep == CD_AUDIO_PLAY_STEP_DONE)) {
        progress->stopStep = CD_AUDIO_STOP_STEP_DONE;
        return -2;
    }
    if (progress->openStep != CD_AUDIO_OPEN_STEP_DONE) {
        progress->cancelRequested = 1;
        return -1;
    }
    if (_gCdAudioState.playback.driver == CD_AUDIO_DRIVER_LOAD_WAVES) {
        if (progress->waveLoadStep != CD_AUDIO_WAVE_LOAD_STEP_NONE) {
            progress->waveLoadStep = CD_AUDIO_WAVE_LOAD_STEP_PAUSE;
            return -2;
        }
    }
    if (CdAudio_Phase.stopStep != CD_AUDIO_STOP_STEP_NONE) {
        return 1;
    }
    CdAudio_StartVolumeRamp(0x20);
    return 0;
}

static s32 CdAudio_DriveStream(void)
{
    volatile CdAudioProgress* progress;
    volatile _CdAudioState*   state;
    CdStreamParams*           params;
    CdlLOC*                   loc;
    s8                        i;
    s8                        status;
    s16                       volume;

    params = (CdStreamParams*)&_gCdAudioState.streamParams;
    switch (CdAudio_Phase.openStep) {
        case CD_AUDIO_OPEN_STEP_SILENCE_VOICES:
            CdAudio_Ctl.openPending = 0;
            status                  = 0;
            for (i = 0x16; i < 0x18; i++) {
                status += Spu_GetVoiceStatus(i);
                if (status != 0) {
                    Spu_KeyOff(i);
                }
            }
            if (status != 0) {
                break;
            }
            CdAudio_Phase.openStep = CD_AUDIO_OPEN_STEP_START_READ;
            /* fallthrough */
        case CD_AUDIO_OPEN_STEP_START_READ:
            progress = &CdAudio_Phase;
            if (progress->cancelRequested == 1) {
                progress->stopStep     = CD_AUDIO_STOP_STEP_DONE;
                CdAudio_Phase.openStep = CD_AUDIO_OPEN_STEP_DONE;
                break;
            }
            state = &_gCdAudioState;
            loc   = (CdlLOC*)&state->seekLoc;
            CdIntToPos(state->playback.startSector, loc);
            if (D_8008277C != 0) {
                volume = 0;
            } else {
                volume = state->playback.volume;
            }
            params->voiceR                = CD_STREAM_VOICE_NONE;
            params->voiceL                = CD_STREAM_VOICE_NONE;
            params->volume                = volume;
            params->channelCount          = 2;
            params->sectorBuf             = &Fs_CdSector;
            params->spuBase               = state->playback.spuBase;
            params->startSector           = CdPosToInt(loc);
            params->doneCb                = CdAudio_SetLocFlag;
            params->startCb               = NULL;
            params->voiceFreeCb           = NULL;
            state->playback.startReported = 0;
            CdStream_Start(params);
            CdAudio_Phase.openStep = CD_AUDIO_OPEN_STEP_WAIT_READ;
            break;
        case CD_AUDIO_OPEN_STEP_WAIT_READ:
            if (_gCdAudioState.playback.startReported != 0) {
                progress = &CdAudio_Phase;
                if (progress->cancelRequested == 1) {
                    progress->stopStep = CD_AUDIO_STOP_STEP_DONE;
                }
                CdAudio_Phase.openStep                = CD_AUDIO_OPEN_STEP_DONE;
                _gCdAudioState.playback.startReported = 0;
            }
            break;
        case CD_AUDIO_OPEN_STEP_NONE:
        case CD_AUDIO_OPEN_STEP_DONE:
            break;
    }
    return CD_AUDIO_DRIVER_PLAY;
}

/* Alignment pad after CdAudio_DriveStream's 5-entry jump table so CdAudio_DriveSeek's
 * compiler-generated jtbl lands at 0x800141DC. */

static s32 CdAudio_DrivePhase0(void)
{
    volatile CdAudioProgress* progress;
    s16                       ret;
    LinInterp*                ramp;

    progress = &CdAudio_Phase;
    ret      = CD_AUDIO_DRIVER_FADE_OUT;

    switch (progress->stopStep) {
        case CD_AUDIO_STOP_STEP_FADE:
            ramp               = (LinInterp*)&_gCdAudioState.ramp;
            progress->playStep = CD_AUDIO_PLAY_STEP_DONE;
            LinInterp_Step(ramp);
            if (ramp->gain == ramp->targetGain) {
                ramp->enabled = LINEAR_INTERPOLATOR_BYPASS;
                CdStream_SetVolume(0);
                CdStream_Stop();
                progress->stopStep = CD_AUDIO_STOP_STEP_RELEASE_VOICES;
            } else {
                CdStream_SetVolume((s16)LinInterp_Apply(ramp, _gCdAudioState.playback.volume));
            }
            break;
        case CD_AUDIO_STOP_STEP_RELEASE_VOICES:
            // These are the voices the player asked for, which is none: the
            // pair the stream allocated is recorded in the stream itself.
            Spu_KeyOff(_gCdAudioState.streamParams.voiceL);
            Spu_KeyOff(_gCdAudioState.streamParams.voiceR);
            progress->stopStep = CD_AUDIO_STOP_STEP_WAIT_IDLE;
            /* fallthrough */
        case CD_AUDIO_STOP_STEP_WAIT_IDLE:
            if (CdStream_IsBusy() == 0) {
                CdAudio_Phase.stopStep = CD_AUDIO_STOP_STEP_DONE;
                ret                    = CD_AUDIO_DRIVER_IDLE;
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
    u8                             step;
    SectorHdr*                     hdr;
    volatile _CdAudioDriverStatus* driverStatus;
    s32                            status;
    s32                            tmp;

    step = CdAudio_Phase.headerReadStep;
    hdr  = (SectorHdr*)CdAudio_SectorBuffer;

    switch (step) {
        case CD_AUDIO_HEADER_READ_STEP_SET_LOCATION:
        do_setloc:
            CdAudio_Ctl.waitTicks        = 0;
            CdAudio_Phase.headerReadStep = CD_AUDIO_HEADER_READ_STEP_WAIT_SET_LOCATION;
            CdControlF(CdlSetloc, (u8*)&_gCdAudioState.seekLoc);
            break;
        case CD_AUDIO_HEADER_READ_STEP_WAIT_SET_LOCATION:
            driverStatus = &CdAudio_Ctl;
            if (driverStatus->waitTicks < CD_AUDIO_WAIT_TIMEOUT_TICKS) {
                if (CdSync(1, NULL) == CdlDiskError) {
                    CdFlush();
                    goto do_setloc;
                }
                CdAudio_Phase.headerReadStep = CD_AUDIO_HEADER_READ_STEP_START_READ;
                    /* fallthrough */
                case CD_AUDIO_HEADER_READ_STEP_START_READ:
                    CdAudio_Phase.headerReadStep = CD_AUDIO_HEADER_READ_STEP_WAIT_READ;
                    CdReadyCallback(CdAudio_ReadyCallback);
                    CdAudio_Ctl.waitTicks       = 0;
                    D_80082770                  = 0;
                    CdAudio_Ctl.headerReadError = CD_AUDIO_HEADER_READ_ERROR_NONE;
                    CdControlF(CdlReadN, NULL);
                    break;
            }
            goto timeout;
        case CD_AUDIO_HEADER_READ_STEP_WAIT_READ:
            if (CdSync(1, NULL) == CdlDiskError) {
                CdAudio_Phase.headerReadStep = CD_AUDIO_HEADER_READ_STEP_START_READ;
                CdFlush();
                CdReadyCallback(NULL);
            } else {
                CdAudio_Phase.headerReadStep = CD_AUDIO_HEADER_READ_STEP_WAIT_SECTOR;
                CdAudio_Ctl.readTicks        = 0;
            }
            break;
        case CD_AUDIO_HEADER_READ_STEP_WAIT_SECTOR:
            driverStatus = &CdAudio_Ctl;
            if (driverStatus->headerReadError != CD_AUDIO_HEADER_READ_ERROR_NONE) {
                driverStatus->failedStep  = CdAudio_Phase.headerReadStep;
                driverStatus->failureKind = CD_AUDIO_FAILURE_SECTOR;
                goto error;
            }
            if (driverStatus->waitTicks < CD_AUDIO_WAIT_TIMEOUT_TICKS) {
                if (D_80082770 != 0) {
                    _gCdAudioState.playback.spuBase = D_80068B18[hdr->field_3];
                    CdAudio_Tbl.field_C             = (u16*)(CdAudio_SectorBuffer + hdr->field_2 * 4);
                    CdReadyCallback(NULL);
                    CdAudio_Phase.headerReadStep = CD_AUDIO_HEADER_READ_STEP_PAUSE;
                        /* fallthrough */
                    case CD_AUDIO_HEADER_READ_STEP_PAUSE:
                        CdControlF(CdlPause, NULL);
                        CdAudio_Ctl.waitTicks        = 0;
                        CdAudio_Phase.headerReadStep = CD_AUDIO_HEADER_READ_STEP_WAIT_PAUSE;
                        /* fallthrough */
                    case CD_AUDIO_HEADER_READ_STEP_WAIT_PAUSE:
                        driverStatus = &CdAudio_Ctl;
                        if (driverStatus->waitTicks < CD_AUDIO_WAIT_TIMEOUT_TICKS) {
                            goto do_cdsync;
                        }
                } else {
                    break;
                }
            }
        timeout:
            driverStatus->failedStep  = CdAudio_Phase.headerReadStep;
            driverStatus->failureKind = CD_AUDIO_FAILURE_TIMEOUT;
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
            CdAudio_Phase.headerReadStep = CD_AUDIO_HEADER_READ_STEP_DONE;
            break;
        default:
            tmp                   = CdAudio_Ctl.waitTicks;
            tmp                   = tmp + 1;
            CdAudio_Ctl.waitTicks = tmp;
            return CD_AUDIO_DRIVER_READ_HEADER;
    }

    tmp                   = CdAudio_Ctl.waitTicks;
    tmp                   = tmp + 1;
    CdAudio_Ctl.waitTicks = tmp;
    return CD_AUDIO_DRIVER_READ_HEADER;

error:
    CdAudio_Phase.headerReadStep = CD_AUDIO_HEADER_READ_STEP_FAILED;
    return CD_AUDIO_DRIVER_IDLE;
}

static s32 CdAudio_DriveRead(void)
{
    // Two pointers to the one status block: the mode wait holds its own across
    // the drive query, apart from the one the shared timeout tail stores through.
    volatile _CdAudioDriverStatus* modeWaitStatus;
    volatile _CdAudioDriverStatus* driverStatus;
    volatile CdAudioTbl*           cd;
    s32                            status;
    u8                             mode;
    s32                            ret;

    switch (CdAudio_Phase.waveLoadStep) {
        case CD_AUDIO_WAVE_LOAD_STEP_RELEASE_STREAM:
            // The voices the player asked for, not the stream's allocated pair.
            Spu_KeyOff(_gCdAudioState.streamParams.voiceL);
            Spu_KeyOff(_gCdAudioState.streamParams.voiceR);
            if (CdStream_IsBusy() != 0) {
                break;
            }
        do_setmode:
            CdAudio_Phase.waveLoadStep = CD_AUDIO_WAVE_LOAD_STEP_WAIT_SET_MODE;
            CdAudio_Ctl.waitTicks      = 0;
            mode                       = CdlModeSpeed | CdlModeSize1;
            CdControlF(CdlSetmode, &mode);
            break;
        case CD_AUDIO_WAVE_LOAD_STEP_WAIT_SET_MODE:
            modeWaitStatus = &CdAudio_Ctl;
            if (modeWaitStatus->waitTicks < CD_AUDIO_WAIT_TIMEOUT_TICKS) {
                goto case2_sync;
            }
            modeWaitStatus->failedStep  = CdAudio_Phase.waveLoadStep;
            modeWaitStatus->failureKind = CD_AUDIO_FAILURE_TIMEOUT;
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
            modeWaitStatus->settleTicks = CD_AUDIO_WAVE_LOAD_SETTLE_TICKS;
            CdAudio_Phase.waveLoadStep  = CD_AUDIO_WAVE_LOAD_STEP_SETTLE;
            break;
        case CD_AUDIO_WAVE_LOAD_STEP_SETTLE:
            CdAudio_Ctl.settleTicks = CdAudio_Ctl.settleTicks - 1;
            if (CdAudio_Ctl.settleTicks >= 0) {
                break;
            }
            CdAudio_Phase.waveLoadStep = CD_AUDIO_WAVE_LOAD_STEP_SET_LOCATION;
            break;
        case CD_AUDIO_WAVE_LOAD_STEP_SET_LOCATION:
        do_setloc:
            CdAudio_Ctl.waitTicks = 0;
            CdAudio_Tbl.field_8   = _gCdAudioState.playback.baseSector;
            CdIntToPos(_gCdAudioState.playback.baseSector, (CdlLOC*)&_gCdAudioState.seekLoc);
            CdAudio_Phase.waveLoadStep = CD_AUDIO_WAVE_LOAD_STEP_WAIT_SET_LOCATION;
            CdControlF(CdlSetloc, (u8*)&_gCdAudioState.seekLoc);
            break;
        case CD_AUDIO_WAVE_LOAD_STEP_WAIT_SET_LOCATION:
            driverStatus = &CdAudio_Ctl;
            if (driverStatus->waitTicks < CD_AUDIO_WAIT_TIMEOUT_TICKS) {
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
                CdAudio_Phase.waveLoadStep = CD_AUDIO_WAVE_LOAD_STEP_START_READ;
                break;
            }
            goto timeout;
        case CD_AUDIO_WAVE_LOAD_STEP_START_READ:
            CdAudio_Phase.waveLoadStep = CD_AUDIO_WAVE_LOAD_STEP_WAIT_READ;
            CdAudio_Tbl.field_1        = 0;
            CdAudio_Ctl.waveLoadError  = CD_AUDIO_WAVE_LOAD_ERROR_NONE;
            CdAudio_Ctl.waitTicks      = 0;
            SpuSetTransferStartAddr(CdAudio_Tbl.field_10);
            CdReadyCallback(CdAudio_FeedSector);
            CdControlF(CdlReadN, NULL);
            break;
        case CD_AUDIO_WAVE_LOAD_STEP_WAIT_READ:
            if (CdSync(1, NULL) == CdlDiskError) {
                CdAudio_Phase.waveLoadStep = CD_AUDIO_WAVE_LOAD_STEP_START_READ;
                CdFlush();
                CdReadyCallback(NULL);
            } else {
                CdAudio_Phase.waveLoadStep = CD_AUDIO_WAVE_LOAD_STEP_WAIT_UPLOAD;
                CdAudio_Ctl.readTicks      = 0;
            }
            break;
        case CD_AUDIO_WAVE_LOAD_STEP_WAIT_UPLOAD:
            driverStatus = &CdAudio_Ctl;
            if (driverStatus->waveLoadError != CD_AUDIO_WAVE_LOAD_ERROR_NONE) {
                driverStatus->failedStep  = CdAudio_Phase.waveLoadStep;
                driverStatus->failureKind = CD_AUDIO_FAILURE_SECTOR;
                goto error;
            }
            if (CdAudio_Ctl.waitTicks < CD_AUDIO_WAIT_TIMEOUT_TICKS) {
                if (driverStatus->readTicks < CD_AUDIO_WAIT_TIMEOUT_TICKS) {
                    cd = &CdAudio_Tbl;
                    if (cd->field_1 == 0) {
                        goto case8_inc;
                    }
                    if (cd->field_1 == 1) {
                        goto do_pause;
                    }
                    goto error;
                case8_inc:
                    driverStatus->readTicks = driverStatus->readTicks + 1;
                    break;
                }
                goto timeout;
            }
            goto timeout;
        case CD_AUDIO_WAVE_LOAD_STEP_PAUSE:
        do_pause:
            CdReadyCallback(NULL);
            CdAudio_Ctl.waitTicks      = 0;
            CdAudio_Phase.waveLoadStep = CD_AUDIO_WAVE_LOAD_STEP_WAIT_PAUSE;
            CdControlF(CdlPause, NULL);
            break;
        case CD_AUDIO_WAVE_LOAD_STEP_WAIT_PAUSE:
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
            ret                        = CD_AUDIO_DRIVER_IDLE;
            CdAudio_Phase.waveLoadStep = CD_AUDIO_WAVE_LOAD_STEP_DONE;
            CdAudio_Phase.stopStep     = CD_AUDIO_STOP_STEP_DONE;
            return ret;
        case9_check:
            driverStatus = &CdAudio_Ctl;
        case9_check2:
            driverStatus = &CdAudio_Ctl;
            if (driverStatus->waitTicks < CD_AUDIO_WAIT_TIMEOUT_TICKS) {
                break;
            }
        timeout:
            driverStatus->failedStep  = CdAudio_Phase.waveLoadStep;
            driverStatus->failureKind = CD_AUDIO_FAILURE_TIMEOUT;
        error:
            CdReadyCallback(NULL);
            CdFlush();
            CdControlF(CdlPause, NULL);
            CdAudio_Phase.waveLoadStep = CD_AUDIO_WAVE_LOAD_STEP_RELEASE_STREAM;
            goto do_setmode;
        case CD_AUDIO_WAVE_LOAD_STEP_DONE:
        default:
            break;
    }

    CdAudio_Ctl.waitTicks = CdAudio_Ctl.waitTicks + 1;
    return CD_AUDIO_DRIVER_LOAD_WAVES;
}

static void CdAudio_FeedSector(u8 arg0, u8* unusedResult)
{
    s32                            arg;
    s32                            pos;
    s32                            ret;
    SndLoadState*                  state;
    s32                            spuIdx;
    volatile _CdAudioDriverStatus* driverStatus;
    volatile CdAudioTbl*           cdState;
    FsSector*                      sector;

    sector       = &Fs_CdSector;
    driverStatus = &CdAudio_Ctl;
    if (driverStatus->waveLoadError != CD_AUDIO_WAVE_LOAD_ERROR_NONE) {
        return;
    }
    cdState = &CdAudio_Tbl;
    if (cdState->field_1 != 0) {
        return;
    }
    arg = arg0 & 0xFF;
    if (arg != 1) {
        driverStatus->waveLoadError = CD_AUDIO_WAVE_LOAD_ERROR_NOT_READY;
        return;
    }
    CdGetSector(sector, 3);
    pos = CdPosToInt(&sector->location);
    if (cdState->field_8 != pos) {
        cdState->field_1            = 0xFF;
        driverStatus->waveLoadError = CD_AUDIO_WAVE_LOAD_ERROR_WRONG_SECTOR;
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
            driverStatus->waveLoadError = CD_AUDIO_WAVE_LOAD_ERROR_FIRST_SECTOR;
            return;
        }
        state->sectorBytes = SOUND_LOAD_SECTOR_BYTES;
    } else {
        ret = SndLoad_ProcessSector(sector->words);
        if (ret == SOUND_LOAD_PHASE_DONE) {
            cdState->field_1 = arg;
            CdReadyCallback(0);
        } else if (ret == SOUND_LOAD_PHASE_ERROR) {
            driverStatus->waveLoadError = CD_AUDIO_WAVE_LOAD_ERROR_UPLOAD;
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
    } while (i < sizeof(_gCdAudioState) / sizeof(*p));

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
    volatile _CdAudioDriverStatus* driverStatus;

    D_800827E4                         = 0;
    D_80082754                         = 0;
    driverStatus                       = &CdAudio_Ctl;
    driverStatus->settleTicks          = 0;
    driverStatus->failedStep           = 0;
    driverStatus->failureKind          = CD_AUDIO_FAILURE_NONE;
    _gCdAudioState.playback.baseSector = arg0;
    CdAudio_SectorBuffer               = 0;
    return 0;
}

static s32 CdAudio_SetupStream(void)
{
    u8  mode;
    u8* mem;
    u8* buf;

    CdAudio_Phase.headerReadStep = CD_AUDIO_HEADER_READ_STEP_SET_LOCATION;
    CdIntToPos(_gCdAudioState.playback.baseSector, (CdlLOC*)&_gCdAudioState.seekLoc);
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

    if (CdAudio_Phase.openStep != CD_AUDIO_OPEN_STEP_DONE) {
        return -1;
    }
    if (CdAudio_Phase.stopStep != CD_AUDIO_STOP_STEP_NONE) {
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
    volatile _CdAudioDriverStatus* driverStatus;
    volatile CdAudioProgress*      progress;
    s32                            baseSector;

    D_800827E4 = 0;
    baseSector = _gCdAudioState.playback.baseSector;
    D_80082754 = 0;
    if (baseSector == 0) {
        _gCdAudioState.playback.baseSector = arg0;
    }
    driverStatus                    = &CdAudio_Ctl;
    driverStatus->settleTicks       = 0;
    driverStatus->failedStep        = 0;
    driverStatus->failureKind       = CD_AUDIO_FAILURE_NONE;
    driverStatus->openPending       = 1;
    _gCdAudioState.playback.spuBase = D_80068B1C;
    progress                        = &CdAudio_Phase;
    progress->stopStep              = CD_AUDIO_STOP_STEP_NONE;
    progress->openStep              = CD_AUDIO_OPEN_STEP_NONE;
    progress->playStep              = CD_AUDIO_PLAY_STEP_NONE;
    progress->waveLoadStep          = CD_AUDIO_WAVE_LOAD_STEP_NONE;
    progress->cancelRequested       = 0;
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
    SpuVoiceRef voiceRef;
    s32*        dest;
    const s32*  arg1;
    u32         i;

    Spu_SetVoiceCallbacks(arg0, NULL, NULL);
    Spu_GetVoiceRef(arg0, &voiceRef);
    dest = (s32*)voiceRef.attr;
    arg1 = (const s32*)attr;
    i    = 0;
    do {
        *dest = *arg1;
        arg1++;
        i++;
        dest++;
    } while (i < sizeof(SpuVoiceAttr) / sizeof(*dest));
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
    s32 playStep;

    playStep = CdAudio_Phase.playStep;
    if ((playStep == CD_AUDIO_PLAY_STEP_DONE) || (playStep == CD_AUDIO_PLAY_STEP_NONE)) {
        _gCdAudioState.playback.startSector = arg0;
        _gCdAudioState.playback.driver      = CD_AUDIO_DRIVER_PLAY;
        CdAudio_Phase.openStep              = CD_AUDIO_OPEN_STEP_SILENCE_VOICES;
    }
    return 0;
}

static s32 CdAudio_RequestStop(void)
{
    volatile CdAudioProgress* progress;
    s32                       ret;

    progress = &CdAudio_Phase;
    if (progress->openStep != CD_AUDIO_OPEN_STEP_DONE) {
        ret                = -1;
        progress->playStep = CD_AUDIO_PLAY_STEP_DONE;
        progress->stopStep = CD_AUDIO_STOP_STEP_FADE;
    } else {
        ret                            = 0;
        progress->stopStep             = CD_AUDIO_STOP_STEP_NONE;
        progress->playStep             = CD_AUDIO_PLAY_STEP_START;
        _gCdAudioState.playback.driver = CD_AUDIO_DRIVER_STOP;
    }
    return ret;
}

static void CdAudio_StartVolumeRamp(s32 arg0)
{
    LinInterp* ramp;

    ramp = (LinInterp*)&_gCdAudioState.ramp;
    LinInterp_Setup(ramp, (_gCdAudioState.playback.volume >> CD_AUDIO_VOLUME_LEVEL_SHIFT) & 0xFF, 0, arg0);
    CdAudio_Phase.playStep         = CD_AUDIO_PLAY_STEP_DONE;
    CdAudio_Phase.stopStep         = CD_AUDIO_STOP_STEP_FADE;
    _gCdAudioState.playback.driver = CD_AUDIO_DRIVER_FADE_OUT;
}

static void CdAudio_JumpWithPitch(s32 arg0, s32 arg1)
{
    _gCdAudioState.playback.baseSector = arg0;
    CdAudio_Tbl.field_10               = arg1;
    CdAudio_Phase.waveLoadStep         = CD_AUDIO_WAVE_LOAD_STEP_RELEASE_STREAM;
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
    switch (CdAudio_Phase.playStep) {
        case CD_AUDIO_PLAY_STEP_NONE:
            break;
        case CD_AUDIO_PLAY_STEP_START:
        case 2: // nothing stores this step
            CdStream_ArmSpuIrq();
            CdAudio_Phase.playStep = CD_AUDIO_PLAY_STEP_WAIT_END;
            break;
        case CD_AUDIO_PLAY_STEP_WAIT_END:
            if (CdStream_IsBusy() == 0) {
                ret                    = CD_AUDIO_DRIVER_FADE_OUT;
                CdAudio_Phase.playStep = CD_AUDIO_PLAY_STEP_DONE;
                CdAudio_Phase.stopStep = CD_AUDIO_STOP_STEP_RELEASE_VOICES;
            }
            break;
        case CD_AUDIO_PLAY_STEP_DONE:
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
            CdAudio_Ctl.headerReadError = CD_AUDIO_HEADER_READ_ERROR_WRONG_SECTOR;
        }
        CdGetSector(CdAudio_SectorBuffer, 0x200);
        D_80082770 = temp;
    } else {
        CdAudio_Ctl.headerReadError = CD_AUDIO_HEADER_READ_ERROR_NOT_READY;
    }
}

static void CdAudio_SetLocFlag(s32 unused)
{
    _gCdAudioState.playback.startReported = 1;
}
