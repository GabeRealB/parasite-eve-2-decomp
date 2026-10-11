#include "main/fs.h"

#include <psyq/sys/types.h>
#include <psyq/libcd.h>
#include <psyq/libetc.h>
#include <psyq/libspu.h>

#include "common.h"

#include "cdvol.h"
#include "cdstream.h"
#include "fs.h"
#include "main/sound.h"
#include "sound.h"
#include "sound_types.h"

/// Seven-bit CD gain levels occupy bits 8..14 of the signed SPU volume word.
enum {
    CD_VOLUME_LEVEL_MASK     = 0x7F,
    CD_VOLUME_LEVEL_SILENT   = 0,
    CD_VOLUME_REGISTER_SHIFT = 8,
    CD_VOLUME_REGISTER_SCALE = 1 << CD_VOLUME_REGISTER_SHIFT,
};

/// Twelve-byte disc-sector header consumed before a sound-bank payload.
typedef struct {
    CdlLOC location;       // BCD position at the start of the sector header
    u8     unreadBytes[8]; // Remaining header bytes; not interpreted here
} _FsSoundBankSectorHeader;

/// Settling polls completed since the resident disc-init job accepted read mode.
///
/// Reset to zero at mode acceptance; four polls finish the job. The poll callback
/// narrows the increment through a signed halfword before testing completion.
static u16 _gCdSyncDiscInitSettlePolls;

/// Resident output selection (0 mono, 1 stereo), shared by all sound paths.
///
/// Starts at zero in BSS; the output setter stores only the request's low bit
/// before refreshing MIDI, script, streaming and CD-input gains.
static u8 _gSndOutputStereoEnabled;

/// BSS bytes with no established use; their original type and role are unproven.
static u8 D_8006EBC0[8];

/// Software mirror of the SPU common output attributes last submitted here.
///
/// Reset seeds master, CD and external-input controls. Later CD-volume writes
/// select only the two CD gain bits in `mask`, retaining the other fields;
/// reading this record does not query hardware. It is separate from the fade cursor.
static SpuCommonAttr _gSpuCommonOutputAttr;

/// Reset-only halfword storage; its role is unproven and no reader is present.
static s16 D5B498_8006EBF0;

/// Retained one-based async-queue cancellation handle for resident disc initialization.
///
/// Values 1..4 identify a queue slot; zero records enqueue failure or completion.
/// There is no current reader, and the standalone enqueue entry has no callers.
static s16 _gCdSyncDiscInitHandle;

/// CD-input fade cursor in seven-bit gain levels (0 silent, 127 full).
///
/// Fade start captures the software CD gain; movie presets replace the cursor
/// and gain together. Each fade step subtracts eight, clamps to zero and applies
/// the level at 256 SPU register units per level. Volatile accesses are retained.
static volatile s32 _gCdVolFadeLevel;

/// BSS bytes with no established use; their original type and role are unproven.
static u8 D_8006EBF8[8];

static void _cdSyncCompleteDiscInit(AsyncCbEntry* entry);

static s32 _cdSyncCancelDiscInit(AsyncCbEntry* entry);

static void _cdVolSetLevel(s32 level);

/// Applies equal signed SPU register gains to the retained CD-input attributes.
///
/// `registerVolume` is already in hardware units; this helper does no level
/// scaling or clamping. Callers must select both CD-volume mask bits first.
/// Other fields are retained and the separate fade cursor is unchanged.
static inline void _cdVolApplyRegisterVolume(s16 registerVolume)
{
    _gSpuCommonOutputAttr.cd.volume.right = registerVolume;
    _gSpuCommonOutputAttr.cd.volume.left  = registerVolume;
    SpuSetCommonAttr(&_gSpuCommonOutputAttr);
}

void fsSoundBankReadyCallback(u8 interruptStatus, u8* unusedResult)
{
    _FsSoundBankSectorHeader sectorHeader;
    SndLoadState*            loadState;
    u32*                     payloadWords;
    s32                      loadPhase;
    s32                      sectorPosition;
    u8                       errorCount;

    /// Reads a header and writes its absolute sector without consuming payload.
    ///
    /// `header` must be a side-effect-free `_FsSoundBankSectorHeader` lvalue;
    /// it is evaluated twice, plus once in sizeof. `position` is assigned once
    /// after both synchronous SDK calls. The transfer count is in u32 words.
#define FILE_SYSTEM_READ_SOUND_BANK_SECTOR_POSITION(header, position) \
    do {                                                              \
        CdGetSector(&(header), sizeof(header) / sizeof(u32));         \
        (position) = CdPosToInt(&(header).location);                  \
    } while (0)

    loadState             = &gSndLoadState;
    loadState->syncUpload = 0;
    if (interruptStatus != CdlDiskError) {
        // Validate the absolute sector before consuming its sound-bank payload.
        FILE_SYSTEM_READ_SOUND_BANK_SECTOR_POSITION(sectorHeader, sectorPosition);
        if (sectorPosition != gFsNextReadSector) {
            errorCount           = gFsCdErrorCount;
            gFsCdOperationStatus = FILE_SYSTEM_CD_OPERATION_RESTART_REQUEST;
            gFsCdErrorCount      = errorCount + 1;
            CdControlF(CdlPause, NULL);
            CdReadyCallback(NULL);
            return;
        }
        gFsCdProgressVBlank = VSync(-1);
        payloadWords        = loadState->sectorBuffer;
        gFsNextReadSector  += 1;
        CdGetSector(payloadWords, FS_SECTOR_WORD_SIZE);
        loadPhase = sndLoadFeedSector(payloadWords);
        if (loadPhase != SOUND_LOAD_RESULT_ERROR) {
            if (loadPhase != SOUND_LOAD_PHASE_DONE) {
                return;
            }
            // Stop sector delivery before binding the completed sequence.
            CdControlF(CdlPause, NULL);
            sndLoadInstallSequence(loadState);
            gFsCdOperationStatus = FILE_SYSTEM_CD_OPERATION_COMPLETE;
            CdReadyCallback(NULL);
            return;
        }
    }
    gFsCdErrorCount += 1;
    CdControlF(CdlPause, NULL);
    gFsCdOperationStatus = FILE_SYSTEM_CD_OPERATION_RESTART_REQUEST;
    CdReadyCallback(NULL);
#undef FILE_SYSTEM_READ_SOUND_BANK_SECTOR_POSITION
}

/// Polls drive initialization through standby, track query, origin seek and read mode.
///
/// The asynchronous queue owns `entry`. Returns zero while pending and one
/// after four settling polls. Uses the shared drive command channel without a
/// timeout; the standalone enqueue entry currently has no callers.
static s32 _cdSyncPollDiscInit(AsyncCbEntry* entry)
{
    struct {
        u8     commandResult[8];
        s8     readMode;
        u8     unknownBytes[7]; // Unaccessed stack bytes; role unproven
        CdlLOC location;
    } scratch;
    enum {
        CD_SYNC_DISC_INIT_WAIT_STANDBY      = 1,
        CD_SYNC_DISC_INIT_GET_TRACKS        = 2,
        CD_SYNC_DISC_INIT_TRACKS_SYNC       = 3,
        CD_SYNC_DISC_INIT_SEEK_ORIGIN       = 4,
        CD_SYNC_DISC_INIT_SEEK_SYNC         = 5,
        CD_SYNC_DISC_INIT_SET_MODE          = 6,
        CD_SYNC_DISC_INIT_SETTLE            = 7,
        CD_SYNC_DISC_INIT_RESTART_ERROR_BIT = 0x40,
        CD_SYNC_DISC_INIT_SETTLE_POLLS      = 4,
        CD_SYNC_DISC_INIT_SYNC_POLL         = 1,
    };
    s32 syncStatus;
    s16 settlePolls;

    if (entry->status.firstPoll) {
        entry->status.firstPoll = 0;
        entry->status.pollState = CD_SYNC_DISC_INIT_WAIT_STANDBY;
    }

    switch (entry->status.pollState) {
        case CD_SYNC_DISC_INIT_WAIT_STANDBY:
            if (CdControlB(CdlNop, NULL, scratch.commandResult) == 0) {
                return CD_SYNC_PENDING;
            }
            if (scratch.commandResult[0] & CdlStatShellOpen) {
                return CD_SYNC_PENDING;
            }
            if (scratch.commandResult[0] & CdlStatStandby) {
                entry->status.pollState = CD_SYNC_DISC_INIT_GET_TRACKS;
                case CD_SYNC_DISC_INIT_GET_TRACKS:
                    if (CdControlB(CdlGetTN, NULL, scratch.commandResult) != 0) {
                        // The stored resume state skips track sync after the immediate poll.
                        entry->status.pollState = CD_SYNC_DISC_INIT_SEEK_ORIGIN;
                        case CD_SYNC_DISC_INIT_TRACKS_SYNC:
                            syncStatus = CdSync(CD_SYNC_DISC_INIT_SYNC_POLL, scratch.commandResult);
                            if (syncStatus == CdlDiskError) {
                                entry->status.pollState = CD_SYNC_DISC_INIT_GET_TRACKS;
                            } else if (syncStatus == CdlComplete) {
                                entry->status.pollState = CD_SYNC_DISC_INIT_SEEK_ORIGIN;
                                case CD_SYNC_DISC_INIT_SEEK_ORIGIN:
                                    CdIntToPos(0, &scratch.location);
                                    if (CdControl(CdlSeekL, &scratch.location.minute, scratch.commandResult) != 0) {
                                        entry->status.pollState = CD_SYNC_DISC_INIT_SEEK_SYNC;
                                        case CD_SYNC_DISC_INIT_SEEK_SYNC:
                                            syncStatus = CdSync(CD_SYNC_DISC_INIT_SYNC_POLL, scratch.commandResult);
                                            // Seek polling advances on a non-complete result unless this error restarts it.
                                            if ((syncStatus == CdlDiskError) && (scratch.commandResult[0] & CdlStatError) &&
                                                (scratch.commandResult[1] & CD_SYNC_DISC_INIT_RESTART_ERROR_BIT)) {
                                                entry->status.pollState = CD_SYNC_DISC_INIT_WAIT_STANDBY;
                                            } else if (syncStatus != CdlComplete) {
                                                entry->status.pollState = CD_SYNC_DISC_INIT_SET_MODE;
                                                case CD_SYNC_DISC_INIT_SET_MODE:
                                                    scratch.readMode = (s8)(CdlModeSpeed | CdlModeSize1);
                                                    if (CdControl(CdlSetmode, (u8*)&scratch.readMode, NULL) != 0) {
                                                        _gCdSyncDiscInitSettlePolls = 0;
                                                        entry->status.pollState     = CD_SYNC_DISC_INIT_SETTLE;
                                                    }
                                            }
                                    }
                            }
                    }
            }
            break;
        case CD_SYNC_DISC_INIT_SETTLE:
            settlePolls                 = _gCdSyncDiscInitSettlePolls + 1;
            _gCdSyncDiscInitSettlePolls = settlePolls;
            if (settlePolls >= CD_SYNC_DISC_INIT_SETTLE_POLLS) {
                return CD_SYNC_COMPLETE;
            }
            break;
    }
    return CD_SYNC_PENDING;
}

void spuResetCommonOutput(void)
{
    enum {
        SPU_OUTPUT_ALL_COMMON_ATTRIBUTES = 0,
        SPU_OUTPUT_MASTER_GAIN           = 0x3FFF,
        SPU_OUTPUT_INPUT_SILENT          = 0,
    };

    _gSpuCommonOutputAttr.mask = SPU_OUTPUT_ALL_COMMON_ATTRIBUTES;

    // Restore direct master gain and keep the silent CD input in the mix.
    _gSpuCommonOutputAttr.mvol.left       = SPU_OUTPUT_MASTER_GAIN;
    _gSpuCommonOutputAttr.mvol.right      = SPU_OUTPUT_MASTER_GAIN;
    _gSpuCommonOutputAttr.mvolmode.left   = SPU_VOICE_DIRECT;
    _gSpuCommonOutputAttr.mvolmode.right  = SPU_VOICE_DIRECT;
    _gSpuCommonOutputAttr.cd.volume.left  = SPU_OUTPUT_INPUT_SILENT;
    _gSpuCommonOutputAttr.cd.volume.right = SPU_OUTPUT_INPUT_SILENT;
    _gSpuCommonOutputAttr.cd.reverb       = SPU_OFF;
    _gSpuCommonOutputAttr.cd.mix          = SPU_ON;

    // Disable the external input and its reverb send.
    _gSpuCommonOutputAttr.ext.volume.left  = SPU_OUTPUT_INPUT_SILENT;
    _gSpuCommonOutputAttr.ext.volume.right = SPU_OUTPUT_INPUT_SILENT;
    _gSpuCommonOutputAttr.ext.reverb       = SPU_OFF;
    _gSpuCommonOutputAttr.ext.mix          = SPU_OFF;

    SpuSetCommonAttr(&_gSpuCommonOutputAttr);
    D5B498_8006EBF0 = 0;
}

void sndOutputSetStereo(s32 enabled)
{
    enum {
        SOUND_OUTPUT_CD_MONO_GAIN    = 90,
        SOUND_OUTPUT_CD_STEREO_GAIN  = 120,
        SOUND_OUTPUT_CD_ROUTE_SILENT = 0
    };
    CdlATV cdMix;
    s32    stereoEnabled;

    /// Configures and applies all four CD-input attenuator gains.
    ///
    /// `mix` must be a side-effect-free `CdlATV` lvalue: it is evaluated five
    /// times. `stereo` is evaluated once; zero selects mono. Uses the gain
    /// constants above and the SDK's `CdMix` declaration.
#define SOUND_OUTPUT_APPLY_CD_MIX(mix, stereo)         \
    do {                                               \
        if ((stereo) == SOUND_OUTPUT_MONO) {           \
            (mix).val0 = SOUND_OUTPUT_CD_MONO_GAIN;    \
            (mix).val1 = SOUND_OUTPUT_CD_MONO_GAIN;    \
            (mix).val2 = SOUND_OUTPUT_CD_MONO_GAIN;    \
            (mix).val3 = SOUND_OUTPUT_CD_MONO_GAIN;    \
        } else {                                       \
            (mix).val0 = SOUND_OUTPUT_CD_STEREO_GAIN;  \
            (mix).val1 = SOUND_OUTPUT_CD_ROUTE_SILENT; \
            (mix).val2 = SOUND_OUTPUT_CD_STEREO_GAIN;  \
            (mix).val3 = SOUND_OUTPUT_CD_ROUTE_SILENT; \
        }                                              \
        CdMix(&(mix));                                 \
    } while (0)

    // Publish the selection before requesting voice-volume refreshes.
    _gSndOutputStereoEnabled = enabled & SOUND_OUTPUT_STEREO;
    stereoEnabled            = (s8)_gSndOutputStereoEnabled;
    midiSetMasterVolume(midiGetMasterVolume() & 0xFF);
    stereoEnabled = (u8)stereoEnabled;
    sndScriptSetMasterVolume(sndScriptGetMasterVolume());
    cdStreamSetMono(stereoEnabled ^ SOUND_OUTPUT_STEREO);
    SOUND_OUTPUT_APPLY_CD_MIX(cdMix, stereoEnabled);
#undef SOUND_OUTPUT_APPLY_CD_MIX
}

u8 sndOutputIsStereo(void)
{
    return _gSndOutputStereoEnabled;
}

void cdVolBeginFadeOut(void)
{
    _gCdVolFadeLevel = (_gSpuCommonOutputAttr.cd.volume.left / CD_VOLUME_REGISTER_SCALE) & CD_VOLUME_LEVEL_MASK;
}

/// Queues drive initialization and retains its one-based cancellation handle.
///
/// Only the three handlers are copied; the temporary entry's other fields are
/// unread. Zero records a full queue. The caller must serialize drive recovery;
/// this standalone entry currently has no callers.
static void _cdSyncQueueDiscInit(void)
{
    AsyncCbEntry callbacks;
    s16*         handle;

    handle             = &_gCdSyncDiscInitHandle;
    callbacks.pollFn   = _cdSyncPollDiscInit;
    callbacks.doneFn   = _cdSyncCompleteDiscInit;
    callbacks.cancelFn = _cdSyncCancelDiscInit;
    *handle            = asyncCbEnqueue(&callbacks);
}

/// Clears the stored disc-initialization handle when its queued job finishes.
///
/// `entry` is the queue-owned completed job and is unused. Zero means no
/// outstanding initialization; the queue itself retires the entry afterwards.
static void _cdSyncCompleteDiscInit(AsyncCbEntry* entry)
{
    enum { CD_SYNC_DISC_INIT_HANDLE_NONE = 0 };

    _gCdSyncDiscInitHandle = CD_SYNC_DISC_INIT_HANDLE_NONE;
}

/// Flushes CD library state when a disc-initialization job is cancelled.
///
/// `entry` is the queue-owned job and is unused. Returning zero completes
/// cancellation in this poll, allowing the queue to retire the job.
static s32 _cdSyncCancelDiscInit(AsyncCbEntry* entry)
{
    enum { CD_SYNC_CANCEL_COMPLETE = 0 };

    CdFlush();
    return CD_SYNC_CANCEL_COMPLETE;
}

/// Returns the retained left CD gain as a seven-bit level (0..127).
///
/// Reads the software attributes last applied by this module, without querying
/// hardware. Division by 256 precedes masking and retains signed truncation.
/// This standalone entry currently has no callers.
static s32 _cdVolGetLevel(void)
{
    return (_gSpuCommonOutputAttr.cd.volume.left / CD_VOLUME_REGISTER_SCALE) & CD_VOLUME_LEVEL_MASK;
}

void cdVolApplyMoviePreset(u16 presetIndex)
{
    enum {
        CD_VOLUME_MOVIE_SILENT_PRESET = 0,
    };

    if (presetIndex >= ARRAY_SIZE(gCdVolMoviePresetLevels)) {
        presetIndex = CD_VOLUME_MOVIE_SILENT_PRESET;
    }
    _gCdVolFadeLevel = gCdVolMoviePresetLevels[presetIndex];
    _cdVolSetLevel(gCdVolMoviePresetLevels[presetIndex]);
}

/// Applies the low seven bits of `level` equally to the SPU's CD inputs.
///
/// One level unit is 256 register units. Other common attributes are excluded
/// by the mask, and the separate fade cursor is unchanged.
static void _cdVolSetLevel(s32 level)
{
    s16 registerVolume;

    _gSpuCommonOutputAttr.mask = SPU_COMMON_CDVOLL | SPU_COMMON_CDVOLR;
    registerVolume             = (level & CD_VOLUME_LEVEL_MASK) << CD_VOLUME_REGISTER_SHIFT;
    _cdVolApplyRegisterVolume(registerVolume);
}

s32 cdVolStepFadeOut(void)
{
    enum { CD_VOLUME_FADE_STEP_LEVELS = 8 };
    s16 registerVolume;

    _gCdVolFadeLevel -= CD_VOLUME_FADE_STEP_LEVELS;
    if (_gCdVolFadeLevel < CD_VOLUME_LEVEL_SILENT) {
        _gCdVolFadeLevel = CD_VOLUME_LEVEL_SILENT;
    }
    _gSpuCommonOutputAttr.mask = SPU_COMMON_CDVOLL | SPU_COMMON_CDVOLR;
    registerVolume             = (_gCdVolFadeLevel & CD_VOLUME_LEVEL_MASK) << CD_VOLUME_REGISTER_SHIFT;
    _cdVolApplyRegisterVolume(registerVolume);
    return _gCdVolFadeLevel;
}
