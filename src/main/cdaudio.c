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

/// `_CdAudioPlayback::driver` values: the `CdAudio_DriveFns` entry `cdAudioPollDriver`
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

/// Number of 32-bit words delivered before a CD sector's payload.
enum { CD_AUDIO_SECTOR_HEADER_WORDS = 3 };

/// libcd synchronization mode that polls a command without waiting.
enum { CD_AUDIO_SYNC_POLL = 1 };

/// CD-audio fade duration in main-loop audio-driver updates.
enum { CD_AUDIO_FADE_OUT_UPDATES = 32 };

/// SPU voice range reserved for the CD stream and polled before opening a track.
enum {
    CD_AUDIO_RESERVED_VOICE_FIRST = 22,
    CD_AUDIO_RESERVED_VOICE_COUNT = 2,
};

/// Serialized lead skipped before the first sector's wave samples.
///
/// Only the sector count, sample length and destination selector are interpreted.
/// The remaining bytes' widths and roles are unproven. Samples begin after
/// `SOUND_LOAD_WAVE_LEAD_BYTES`, and later sectors contain only sample bytes.
typedef struct {
    u8  transferSectors;                                         // Copied to the sound loader's failed-load drain threshold, in sectors
    u8  unknown_1[3];                                            // Unread bytes; widths and roles unproven
    u32 sampleBytes;                                             // Total sample bytes to upload, stored in the loader's signed remaining-byte count
    u32 spuAddressIndex;                                         // 0 keeps the requested SPU address; nonzero indexes the two-entry destination table unchecked
    u8  unknown_C[SOUND_LOAD_WAVE_LEAD_BYTES - 3 * sizeof(u32)]; // Unread remainder of the lead; widths and roles unproven
} _CdAudioWaveLead;
STATIC_ASSERT_SIZEOF(_CdAudioWaveLead, SOUND_LOAD_WAVE_LEAD_BYTES);

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

/// `_CdAudioReadState::waveLoadResult` values: how the wave load's upload
/// stands.
enum {
    CD_AUDIO_WAVE_LOAD_RESULT_RUNNING      = 0,    // the upload is still taking sectors
    CD_AUDIO_WAVE_LOAD_RESULT_DONE         = 1,    // the upload took its last sector
    CD_AUDIO_WAVE_LOAD_RESULT_WRONG_SECTOR = 0xFF, // a sector other than the next one arrived; later ones are ignored
};

/// One slot of the table a header sector carries beside its track entries.
///
/// The header's first word places the table, in 4-byte words from the start of
/// the sector, and a track's entry names a slot by an 8-bit index, so several
/// tracks can share one and every slot an entry can name lies inside the
/// sector. Choosing a track copies its slot's `value` and nothing uses the
/// copy, so what a slot describes is unproven.
typedef struct {
    u16 value;   // copied to `_CdAudioReadState::trackSlotValue` when a track naming the slot is chosen; meaning unproven
    u16 field_2; // never accessed: the slot's 4-byte size is established, this half's width and role are not
} _CdAudioHeaderSlot;
STATIC_ASSERT_SIZEOF(_CdAudioHeaderSlot, 0x4);

/// The header sector of a set of CD audio tracks, as the player reads it into
/// its sector buffer.
///
/// The sector is 32-bit words throughout: one of header fields, then an entry
/// for each track, then the table of 4-byte slots those entries index. A track
/// is chosen by its entry's position and nothing the player reads counts them,
/// so where the entries end is known only from where the slot table starts.
///
/// No caller starts a header read, so the layout is established from the
/// player's code alone and never from a sector on the disc.
typedef struct {
    u8  field_0[2];          // never accessed; widths and roles unproven
    u8  slotTableOffset;     // where the `_CdAudioHeaderSlot` table starts, in 32-bit words from the start of the sector
    u8  spuBaseIndex;        // entry of the player's SPU address table that gives `_CdAudioPlayback::spuBase` for the set's tracks
    u32 trackEntries[0x1FF]; // the rest of the sector: a packed word per track (sector offset from the header, slot, level, flag), then the slot table
} _CdAudioHeader;
STATIC_ASSERT_SIZEOF(_CdAudioHeader, 0x800);

/// What the CD audio player's two disc reads work from and leave behind: the
/// wave load's position, destination and outcome, and what the header sector
/// gave for the track last chosen from it.
///
/// The player's one instance is volatile: the wave load's fields are shared
/// with its CD ready callback.
typedef struct {
    u8                  trackFlag;          // top bit of the chosen track's header entry; stored only, so what it marks is unproven
    u8                  waveLoadResult;     // `CD_AUDIO_WAVE_LOAD_RESULT_*`
    u8                  field_2;            // read as an index into a table the player never sets up, and never stored; role unproven
    s32                 trackSlotValue;     // `_CdAudioHeaderSlot::value` of the slot the chosen track's entry names; stored only, so its meaning is unproven
    s32                 waveLoadNextSector; // absolute disc sector the wave load takes next; any other sector delivered fails the load
    _CdAudioHeaderSlot* slotTable;          // the slot table of the header sector last read, inside the sector buffer
    s32                 waveLoadSpuAddress; // SPU address the upload is pointed at when the wave load's read starts
    s32                 field_14;           // nothing reads or writes it; role unproven
} _CdAudioReadState;
STATIC_ASSERT_SIZEOF(_CdAudioReadState, 0x18);

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

/// One entry of a table the CD audio player would pick a track from; what the
/// table holds is unproven.
///
/// The player keeps a pointer to an array of these and never sets it, and the
/// one routine that reads through it has no caller. That routine takes the
/// entry at an index and the one after it, so entries are four bytes apart and
/// the array runs at least one entry past any index used.
typedef struct {
    u8 field_0[3]; // never accessed: the entry's 4-byte size is established, these bytes' widths and roles are not
    u8 field_3;    // the next entry's less this one's, less 1, is the header track the unreached routine selects; role unproven
} _CdAudioTableEntry;
STATIC_ASSERT_SIZEOF(_CdAudioTableEntry, 0x4);

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

/* Define BSS before API headers to preserve first-declaration order. */
static u8* CdAudio_SectorBuffer;

static u8 D_80082754;

static volatile _CdAudioReadState CdAudio_Tbl;

static volatile s32 D_80082770;

/// Unreferenced.
static u8 D_80082774[4];

static u32* CdAudio_SectorEntries;

static volatile u8 D_8008277C;

static volatile _CdAudioDriverStatus CdAudio_Ctl;

static _CdAudioTableEntry* CdAudio_TblEntries;

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

static s32 _cdAudioOpenDriver(void);

static s32 _cdAudioStopDriver(void);

static s32 _cdAudioReadHeaderDriver(void);

static s32 _cdAudioLoadWavesDriver(void);

static void _cdAudioWaveSectorReadyCallback(u8 interruptStatus, u8* unusedResult);

static u8 _cdAudioGetDriver(void);

static s32 _cdAudioFadeOutTableTrack(void);

static s32 _cdAudioResetTrack(s32 baseSector);

static s32 _cdAudioOpenTrackIfPresent(s32 startSector);

static s32 _cdAudioSelectHeaderTrack(s32 trackIndex);

static s32 _cdAudioOpenTrackAtSector(s32 startSector);

static s32 _cdAudioRequestPlay(void);

static void _cdAudioStartFadeOut(s32 updateCount);

static void _cdAudioStartWaveLoad(s32 firstSector, s32 spuAddress);

static s32 _cdAudioIdleDriver(void);

static s32 _cdAudioPlayDriver(void);

static void _cdAudioHeaderSectorReadyCallback(u8 interruptStatus, u8* unusedResult);

static void _cdAudioStreamOpeningDoneCallback(s32 unusedOpened);

/// Consumes the drive's twelve-byte sector header and returns its absolute disc sector.
///
/// `sector` is borrowed writable, word-aligned scratch storage. Only its first
/// twelve bytes are replaced; the BCD location there is converted to a sector
/// number. The caller must read the payload afterwards, before the next sector.
static inline s32 _cdAudioReadSectorPosition(FsSector* sector)
{
    CdGetSector(sector, CD_AUDIO_SECTOR_HEADER_WORDS);
    return CdPosToInt(&sector->location);
}

/// Reads a header-sector position and binds the playback state used to validate it.
///
/// sector is a writable FsSector pointer evaluated twice. playbackState and
/// sectorPosition are plain local output lvalues evaluated once; none of the
/// arguments may have side effects. The captured player state is the owner of
/// the expected sector. The payload read and mismatch handling stay in the caller.
#define CD_AUDIO_READ_HEADER_SECTOR_POSITION(sector, playbackState, sectorPosition) \
    do {                                                                            \
        CdGetSector((sector), CD_AUDIO_SECTOR_HEADER_WORDS);                        \
        (playbackState)  = &_gCdAudioState.playback;                                \
        (sectorPosition) = CdPosToInt(&(sector)->location);                         \
    } while (0)

/// Copies a complete SDK voice-attribute record, including its representation bytes.
///
/// Both arguments are borrowed, word-aligned complete records; `destination`
/// must be writable. Copies forward one word at a time, replacing the voice
/// bit, update mask and every attribute rather than merging pending edits.
static inline void _spuCopyVoiceAttributes(SpuVoiceAttr* destination, const SpuVoiceAttr* source)
{
    s32*       destinationWords = (s32*)destination;
    const s32* sourceWords      = (const s32*)source;
    u32        wordsCopied;

    for (wordsCopied = 0; wordsCopied < sizeof(*destination) / sizeof(*destinationWords); wordsCopied++) {
        *destinationWords = *sourceWords;
        sourceWords++;
        destinationWords++;
    }
}

/// Resets the shared sound loader's transfer state for a CD-audio waveform upload.
///
/// Both arguments are borrowed for the call: `loadState` is writable and `waveLead`
/// is the first sector's complete serialized lead. The first feed starts after
/// that lead and contributes at most `SOUND_LOAD_WAVE_FIRST_BYTES` sample bytes.
/// DMA completion is polled, and a busy DMA leaves the existing bank allocated.
/// The caller selects the SPU destination; bank and image ownership stay intact.
static inline void _cdAudioPrepareWaveUpload(SndLoadState* loadState, const _CdAudioWaveLead* waveLead)
{
    loadState->feedMode                       = SOUND_LOAD_FEED_CD_AUDIO;
    loadState->payload.header.waveBlockOffset = 0;
    loadState->sectorsArrived                 = 0;
    loadState->syncUpload                     = 0;
    loadState->sectorBytes                    = SOUND_LOAD_WAVE_FIRST_BYTES;
    loadState->phase                          = SOUND_LOAD_PHASE_UPLOAD_WAVE;
    loadState->bytesRemaining                 = waveLead->sampleBytes;
    loadState->payload.header.transferSectors = waveLead->transferSectors;
}

s32 cdAudioCancel(void)
{
    volatile CdAudioProgress* progress;

    progress                  = &CdAudio_Phase;
    progress->cancelRequested = 1;
    if (CdAudio_Ctl.openPending == 1) {
        CdAudio_Ctl.openPending = 0;
        return CD_AUDIO_CANCEL_OPEN_PENDING;
    }
    if ((progress->playStep == CD_AUDIO_PLAY_STEP_NONE) || (progress->playStep == CD_AUDIO_PLAY_STEP_DONE)) {
        progress->stopStep = CD_AUDIO_STOP_STEP_DONE;
        return CD_AUDIO_CANCEL_NO_FADE;
    }
    if (progress->openStep != CD_AUDIO_OPEN_STEP_DONE) {
        progress->cancelRequested = 1;
        return CD_AUDIO_CANCEL_OPENING;
    }
    if (_gCdAudioState.playback.driver == CD_AUDIO_DRIVER_LOAD_WAVES) {
        if (progress->waveLoadStep != CD_AUDIO_WAVE_LOAD_STEP_NONE) {
            progress->waveLoadStep = CD_AUDIO_WAVE_LOAD_STEP_PAUSE;
            return CD_AUDIO_CANCEL_NO_FADE;
        }
    }
    if (CdAudio_Phase.stopStep != CD_AUDIO_STOP_STEP_NONE) {
        return CD_AUDIO_CANCEL_STOP_PENDING;
    }
    _cdAudioStartFadeOut(CD_AUDIO_FADE_OUT_UPDATES);
    return CD_AUDIO_CANCEL_FADE_STARTED;
}

/// Queues key-off for the reserved streaming voices and returns their sampled status sum.
///
/// The result is 0..6: zero means both cached statuses are `SPU_OFF`.
/// Any other status, including `SPU_ON_ENV_OFF`, keeps opening waiting for
/// another audio tick. Key-offs take effect at the next SPU flush; this call
/// neither refreshes hardware status nor releases the voices' allocations.
static inline s8 _cdAudioSilenceReservedVoices(void)
{
    s8 voiceIdx;
    s8 keyStatusSum = 0;

    // Keep the cumulative test: an earlier non-silent voice also keys off the later voice.
    for (voiceIdx = CD_AUDIO_RESERVED_VOICE_FIRST; voiceIdx < CD_AUDIO_RESERVED_VOICE_FIRST + CD_AUDIO_RESERVED_VOICE_COUNT; voiceIdx++) {
        keyStatusSum += spuGetVoiceKeyStatus(voiceIdx);
        if (keyStatusSum != SPU_OFF) {
            spuKeyOff(voiceIdx);
        }
    }
    return keyStatusSum;
}

/// Advances track opening, silencing the reserved voices before queuing the first read.
///
/// Services `CdAudio_Phase.openStep` once per audio-driver tick and returns
/// `CD_AUDIO_DRIVER_PLAY`, the dispatch slot used for opening. Completion or
/// abandonment of the opening read both finish this operation; cancellation
/// also marks the stop done. The shared sector buffer and SPU rings must remain
/// available to the stream until all reads and playback have ended.
static s32 _cdAudioOpenDriver(void)
{
    enum {
        CD_AUDIO_INITIAL_CHANNEL_COUNT = 2,
    };
    volatile CdAudioProgress* progress;
    volatile _CdAudioState*   playerState;
    CdStreamParams*           streamParams;
    CdlLOC*                   location;
    s8                        keyStatusSum;
    s16                       volume;

    streamParams = (CdStreamParams*)&_gCdAudioState.streamParams;
    switch (CdAudio_Phase.openStep) {
        case CD_AUDIO_OPEN_STEP_SILENCE_VOICES:
            CdAudio_Ctl.openPending = 0;
            keyStatusSum            = _cdAudioSilenceReservedVoices();
            if (keyStatusSum != SPU_OFF) {
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
            playerState = &_gCdAudioState;
            location    = (CdlLOC*)&playerState->seekLoc;
            CdIntToPos(playerState->playback.startSector, location);
            if (D_8008277C != 0) {
                volume = 0;
            } else {
                volume = playerState->playback.volume;
            }
            streamParams->voiceR                = CD_STREAM_VOICE_NONE;
            streamParams->voiceL                = CD_STREAM_VOICE_NONE;
            streamParams->volume                = volume;
            streamParams->channelCount          = CD_AUDIO_INITIAL_CHANNEL_COUNT;
            streamParams->sectorBuf             = &Fs_CdSector;
            streamParams->spuBase               = playerState->playback.spuBase;
            streamParams->startSector           = CdPosToInt(location);
            streamParams->doneCb                = _cdAudioStreamOpeningDoneCallback;
            streamParams->startCb               = NULL;
            streamParams->voiceFreeCb           = NULL;
            playerState->playback.startReported = 0;
            cdStreamOpen(streamParams);
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

/// Steps the stream's volume ramp and requests shutdown once it reaches silence.
///
/// Borrows writable progress for the active player, once per fade-driver update.
/// The player's ramp must already target zero. Playback is marked done on every
/// call; reaching the target sets zero gain before requesting asynchronous
/// shutdown and selecting the voice-release step. Until then the normalized
/// ramp scales the track's SPU gain, narrowed to the stream's signed halfword.
static inline void _cdAudioAdvanceStopFade(volatile CdAudioProgress* progress)
{
    LinInterp* ramp = (LinInterp*)&_gCdAudioState.ramp;

    progress->playStep = CD_AUDIO_PLAY_STEP_DONE;
    linInterpStep(ramp);
    if (ramp->gain == ramp->targetGain) {
        ramp->enabled = LINEAR_INTERPOLATOR_BYPASS;
        cdStreamSetVolume(0);
        cdStreamStop();
        progress->stopStep = CD_AUDIO_STOP_STEP_RELEASE_VOICES;
    } else {
        cdStreamSetVolume((s16)linInterpApply(ramp, _gCdAudioState.playback.volume));
    }
}

/// Advances volume fade and stream shutdown, then waits for outstanding stream work.
///
/// Services `CdAudio_Phase.stopStep` once per audio-driver tick. Returns
/// `CD_AUDIO_DRIVER_FADE_OUT` while waiting and `CD_AUDIO_DRIVER_IDLE` on
/// completion. Natural playback completion enters at the retained key-off step
/// without a fade; stream shutdown itself owns the live voices' key-off.
static s32 _cdAudioStopDriver(void)
{
    volatile CdAudioProgress* progress;
    s16                       nextDriver;

    progress   = &CdAudio_Phase;
    nextDriver = CD_AUDIO_DRIVER_FADE_OUT;

    switch (progress->stopStep) {
        case CD_AUDIO_STOP_STEP_FADE:
            _cdAudioAdvanceStopFade(progress);
            break;
        case CD_AUDIO_STOP_STEP_RELEASE_VOICES:
            // Retain setup-value key-offs: after the first open both are NONE,
            // selecting bit 31, not a hardware voice; before it both are zero.
            spuKeyOff(_gCdAudioState.streamParams.voiceL);
            spuKeyOff(_gCdAudioState.streamParams.voiceR);
            progress->stopStep = CD_AUDIO_STOP_STEP_WAIT_IDLE;
            /* fallthrough */
        case CD_AUDIO_STOP_STEP_WAIT_IDLE:
            if (cdStreamIsBusy() == 0) {
                CdAudio_Phase.stopStep = CD_AUDIO_STOP_STEP_DONE;
                nextDriver             = CD_AUDIO_DRIVER_IDLE;
            }
            break;
    }
    return nextDriver;
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
    _cdAudioIdleDriver,
    _cdAudioOpenDriver,
    _cdAudioPlayDriver,
    _cdAudioStopDriver,
    _cdAudioIdleDriver,
    _cdAudioReadHeaderDriver,
    _cdAudioLoadWavesDriver,
    _cdAudioIdleDriver,
};

/// Installs the header-sector receiver and starts reading with fresh wait and error state.
///
/// Requires the drive location and mode to be set, and the player's allocated
/// header buffer to remain writable until reception ends. Replaces the libcd
/// ready callback without saving it. The driver polls command acceptance on
/// later ticks; issuing the read here does not establish that a sector arrived.
static inline void _cdAudioStartHeaderSectorRead(void)
{
    CdAudio_Phase.headerReadStep = CD_AUDIO_HEADER_READ_STEP_WAIT_READ;
    CdReadyCallback(_cdAudioHeaderSectorReadyCallback);
    CdAudio_Ctl.waitTicks       = 0;
    D_80082770                  = 0;
    CdAudio_Ctl.headerReadError = CD_AUDIO_HEADER_READ_ERROR_NONE;
    CdControlF(CdlReadN, NULL);
}

/// Advances a header-sector read and retains its track and slot tables for selection.
///
/// Returns `CD_AUDIO_DRIVER_READ_HEADER`, including after completion, or
/// `CD_AUDIO_DRIVER_IDLE` after a timeout or sector error. The allocated sector
/// buffer remains owned by the player. No live caller starts this path; the
/// header's SPU-table index range is unproven. Its byte-sized slot-table offset
/// and each track's byte-sized slot index stay within the sector, including a
/// zero offset. The drive's location was prepared before entry.
static s32 _cdAudioReadHeaderDriver(void)
{
    u8                             step;
    _CdAudioHeader*                header;
    volatile _CdAudioDriverStatus* driverStatus;
    s32                            waitTicks;

    step   = CdAudio_Phase.headerReadStep;
    header = (_CdAudioHeader*)CdAudio_SectorBuffer;

    switch (step) {
        case CD_AUDIO_HEADER_READ_STEP_SET_LOCATION:
        do_setloc:
            CdAudio_Ctl.waitTicks        = 0;
            CdAudio_Phase.headerReadStep = CD_AUDIO_HEADER_READ_STEP_WAIT_SET_LOCATION;
            // libcd consumes the prepared location's bytes; it does not retain this pointer.
            CdControlF(CdlSetloc, (u8*)&_gCdAudioState.seekLoc);
            break;
        case CD_AUDIO_HEADER_READ_STEP_WAIT_SET_LOCATION:
            driverStatus = &CdAudio_Ctl;
            if (driverStatus->waitTicks < CD_AUDIO_WAIT_TIMEOUT_TICKS) {
                if (CdSync(CD_AUDIO_SYNC_POLL, NULL) == CdlDiskError) {
                    CdFlush();
                    goto do_setloc;
                }
                CdAudio_Phase.headerReadStep = CD_AUDIO_HEADER_READ_STEP_START_READ;
                    /* fallthrough */
                case CD_AUDIO_HEADER_READ_STEP_START_READ:
                    _cdAudioStartHeaderSectorRead();
                    break;
            }
            goto timeout;
        case CD_AUDIO_HEADER_READ_STEP_WAIT_READ:
            if (CdSync(CD_AUDIO_SYNC_POLL, NULL) == CdlDiskError) {
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
                    _gCdAudioState.playback.spuBase = D_80068B18[header->spuBaseIndex];
                    CdAudio_Tbl.slotTable           = (_CdAudioHeaderSlot*)&((u32*)CdAudio_SectorBuffer)[header->slotTableOffset];
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
            switch (CdSync(CD_AUDIO_SYNC_POLL, NULL)) {
                case CdlDiskError:
                    CdFlush();
                    /* fallthrough */
                case CdlComplete:
                    CdAudio_Phase.headerReadStep = CD_AUDIO_HEADER_READ_STEP_DONE;
                    break;
                case CdlNoIntr:
                default:
                    break;
            }
            break;
        default:
            waitTicks             = CdAudio_Ctl.waitTicks;
            waitTicks             = waitTicks + 1;
            CdAudio_Ctl.waitTicks = waitTicks;
            return CD_AUDIO_DRIVER_READ_HEADER;
    }

    waitTicks             = CdAudio_Ctl.waitTicks;
    waitTicks             = waitTicks + 1;
    CdAudio_Ctl.waitTicks = waitTicks;
    return CD_AUDIO_DRIVER_READ_HEADER;

error:
    CdAudio_Phase.headerReadStep = CD_AUDIO_HEADER_READ_STEP_FAILED;
    return CD_AUDIO_DRIVER_IDLE;
}

/// Requests double-speed 2340-byte sectors, including their location header, and starts the mode wait.
///
/// Used both on entry to a wave upload and on a retry. The mode byte is borrowed
/// by libcd only during this call. Command acceptance and the settling delay are
/// handled on subsequent driver ticks; this does not start a sector read.
static inline void _cdAudioSetWaveReadMode(void)
{
    u8 driveMode;

    CdAudio_Phase.waveLoadStep = CD_AUDIO_WAVE_LOAD_STEP_WAIT_SET_MODE;
    CdAudio_Ctl.waitTicks      = 0;
    driveMode                  = CdlModeSpeed | CdlModeSize1;
    CdControlF(CdlSetmode, &driveMode);
}

/// Advances a disc-to-SPU waveform upload, restarting failed reads from drive-mode setup.
///
/// Returns `CD_AUDIO_DRIVER_LOAD_WAVES` while working and
/// `CD_AUDIO_DRIVER_IDLE` once the upload and drive pause have completed.
/// Requires the shared sector buffer, sound loader and requested SPU destination
/// to remain available. Entry waits for stream work but does not stop it.
/// The first sector may override the SPU destination through its serialized lead.
static s32 _cdAudioLoadWavesDriver(void)
{
    // Two pointers to the one status block: the mode wait holds its own across
    // the drive query, apart from the one the shared timeout tail stores through.
    volatile _CdAudioDriverStatus* modeWaitStatus;
    volatile _CdAudioDriverStatus* driverStatus;
    volatile _CdAudioReadState*    readState;

    switch (CdAudio_Phase.waveLoadStep) {
        case CD_AUDIO_WAVE_LOAD_STEP_RELEASE_STREAM:
            // Retain setup-value key-offs: zero BSS selects voice 0 twice before
            // any open; afterwards NONE selects bit 31 and no hardware voice.
            spuKeyOff(_gCdAudioState.streamParams.voiceL);
            spuKeyOff(_gCdAudioState.streamParams.voiceR);
            if (cdStreamIsBusy() != 0) {
                break;
            }
        do_setmode:
            _cdAudioSetWaveReadMode();
            break;
        case CD_AUDIO_WAVE_LOAD_STEP_WAIT_SET_MODE:
            modeWaitStatus = &CdAudio_Ctl;
            if (modeWaitStatus->waitTicks >= CD_AUDIO_WAIT_TIMEOUT_TICKS) {
                modeWaitStatus->failedStep  = CdAudio_Phase.waveLoadStep;
                modeWaitStatus->failureKind = CD_AUDIO_FAILURE_TIMEOUT;
                goto error;
            }
            switch (CdSync(CD_AUDIO_SYNC_POLL, NULL)) {
                case CdlDiskError:
                    CdFlush();
                    goto do_setmode;
                case CdlComplete:
                    modeWaitStatus->settleTicks = CD_AUDIO_WAVE_LOAD_SETTLE_TICKS;
                    CdAudio_Phase.waveLoadStep  = CD_AUDIO_WAVE_LOAD_STEP_SETTLE;
                    break;
                case CdlNoIntr:
                default:
                    break;
            }
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
            CdAudio_Ctl.waitTicks          = 0;
            CdAudio_Tbl.waveLoadNextSector = _gCdAudioState.playback.baseSector;
            CdIntToPos(_gCdAudioState.playback.baseSector, (CdlLOC*)&_gCdAudioState.seekLoc);
            CdAudio_Phase.waveLoadStep = CD_AUDIO_WAVE_LOAD_STEP_WAIT_SET_LOCATION;
            CdControlF(CdlSetloc, (u8*)&_gCdAudioState.seekLoc);
            break;
        case CD_AUDIO_WAVE_LOAD_STEP_WAIT_SET_LOCATION:
            driverStatus = &CdAudio_Ctl;
            if (driverStatus->waitTicks < CD_AUDIO_WAIT_TIMEOUT_TICKS) {
                switch (CdSync(CD_AUDIO_SYNC_POLL, NULL)) {
                    case CdlDiskError:
                        CdFlush();
                        goto do_setloc;
                    case CdlComplete:
                        CdAudio_Phase.waveLoadStep = CD_AUDIO_WAVE_LOAD_STEP_START_READ;
                        break;
                    case CdlNoIntr:
                    default:
                        break;
                }
                break;
            }
            goto timeout;
        case CD_AUDIO_WAVE_LOAD_STEP_START_READ:
            // The ready callback validates each sector and feeds the shared sound loader.
            CdAudio_Phase.waveLoadStep = CD_AUDIO_WAVE_LOAD_STEP_WAIT_READ;
            CdAudio_Tbl.waveLoadResult = CD_AUDIO_WAVE_LOAD_RESULT_RUNNING;
            CdAudio_Ctl.waveLoadError  = CD_AUDIO_WAVE_LOAD_ERROR_NONE;
            CdAudio_Ctl.waitTicks      = 0;
            SpuSetTransferStartAddr(CdAudio_Tbl.waveLoadSpuAddress);
            CdReadyCallback(_cdAudioWaveSectorReadyCallback);
            CdControlF(CdlReadN, NULL);
            break;
        case CD_AUDIO_WAVE_LOAD_STEP_WAIT_READ:
            if (CdSync(CD_AUDIO_SYNC_POLL, NULL) == CdlDiskError) {
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
            if (CdAudio_Ctl.waitTicks < CD_AUDIO_WAIT_TIMEOUT_TICKS && driverStatus->readTicks < CD_AUDIO_WAIT_TIMEOUT_TICKS) {
                readState = &CdAudio_Tbl;
                if (readState->waveLoadResult != CD_AUDIO_WAVE_LOAD_RESULT_RUNNING) {
                    if (readState->waveLoadResult == CD_AUDIO_WAVE_LOAD_RESULT_DONE) {
                        goto do_pause;
                    }
                    goto error;
                }
                driverStatus->readTicks = driverStatus->readTicks + 1;
                break;
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
            switch (CdSync(CD_AUDIO_SYNC_POLL, NULL)) {
                case CdlDiskError:
                    CdFlush();
                    goto do_pause;
                case CdlComplete:
                    CdAudio_Phase.waveLoadStep = CD_AUDIO_WAVE_LOAD_STEP_DONE;
                    CdAudio_Phase.stopStep     = CD_AUDIO_STOP_STEP_DONE;
                    return CD_AUDIO_DRIVER_IDLE;
                case CdlNoIntr:
                default:
                    break;
            }
            driverStatus = &CdAudio_Ctl;
            if (driverStatus->waitTicks < CD_AUDIO_WAIT_TIMEOUT_TICKS) {
                break;
            }
        timeout:
            driverStatus->failedStep  = CdAudio_Phase.waveLoadStep;
            driverStatus->failureKind = CD_AUDIO_FAILURE_TIMEOUT;
        error:
            // Retire the callback before flushing and restarting the same upload.
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

/// Validates and uploads the next sector of the active wave load.
///
/// The libcd result buffer is unused. Errors and a completed upload suppress
/// further sectors. The first sector carries a 64-byte lead; later sectors
/// contribute 2048 sample bytes, capped by the loader's remaining-byte count.
static void _cdAudioWaveSectorReadyCallback(u8 interruptStatus, u8* unusedResult)
{
    s32                            readyStatus;
    s32                            sectorPosition;
    s32                            uploadPhase;
    SndLoadState*                  loadState;
    s32                            spuAddressIndex;
    volatile _CdAudioDriverStatus* driverStatus;
    volatile _CdAudioReadState*    readState;
    FsSector*                      sector;
    const _CdAudioWaveLead*        waveLead;

    sector       = &Fs_CdSector;
    driverStatus = &CdAudio_Ctl;
    if (driverStatus->waveLoadError != CD_AUDIO_WAVE_LOAD_ERROR_NONE) {
        return;
    }
    readState = &CdAudio_Tbl;
    if (readState->waveLoadResult != CD_AUDIO_WAVE_LOAD_RESULT_RUNNING) {
        return;
    }
    readyStatus = interruptStatus;
    if (readyStatus != CdlDataReady) {
        driverStatus->waveLoadError = CD_AUDIO_WAVE_LOAD_ERROR_NOT_READY;
        return;
    }
    // The drive's location header is consumed before its payload overwrites it.
    sectorPosition = _cdAudioReadSectorPosition(sector);
    if (readState->waveLoadNextSector != sectorPosition) {
        readState->waveLoadResult   = CD_AUDIO_WAVE_LOAD_RESULT_WRONG_SECTOR;
        driverStatus->waveLoadError = CD_AUDIO_WAVE_LOAD_ERROR_WRONG_SECTOR;
        return;
    }
    CdGetSector(sector, ARRAY_SIZE(sector->words));
    if (_gCdAudioState.playback.baseSector == readState->waveLoadNextSector) {
        // First waveform sector: skip its lead and upload samples.
        // A busy SPU transfer keeps the bank.
        waveLead  = (const _CdAudioWaveLead*)sector->bytes;
        loadState = &SndLoad_State;
        _cdAudioPrepareWaveUpload(loadState, waveLead);
        spuAddressIndex = waveLead->spuAddressIndex;
        if (spuAddressIndex != 0) {
            SpuSetTransferStartAddr(D_80068B2C[spuAddressIndex]);
        }
        if (sndLoadProcessSector(&sector->words[SOUND_LOAD_WAVE_LEAD_BYTES / sizeof(u32)]) == SOUND_LOAD_PHASE_ERROR) {
            driverStatus->waveLoadError = CD_AUDIO_WAVE_LOAD_ERROR_FIRST_SECTOR;
            return;
        }
        loadState->sectorBytes = SOUND_LOAD_SECTOR_BYTES;
    } else {
        uploadPhase = sndLoadProcessSector(sector->words);
        if (uploadPhase == SOUND_LOAD_PHASE_DONE) {
            readState->waveLoadResult = CD_AUDIO_WAVE_LOAD_RESULT_DONE;
            CdReadyCallback(NULL);
        } else if (uploadPhase == SOUND_LOAD_PHASE_ERROR) {
            driverStatus->waveLoadError = CD_AUDIO_WAVE_LOAD_ERROR_UPLOAD;
        }
    }
    CdAudio_Tbl.waveLoadNextSector += 1;
}

/// Repeats a clear of one word without advancing to the rest of the control block.
///
/// `firstWord` is a borrowed, plain representation view of four word-aligned,
/// writable bytes. `repetitions` counts stores to those same bytes, not an extent
/// to clear. A zero count still performs one store; no later word is accessed.
static inline void _cdAudioRepeatFirstWordClear(s32* firstWord, u32 repetitions)
{
    u32 stores = 0;

    do {
        stores++;
        *firstWord = 0;
    } while (stores < repetitions);
}

void cdAudioInit(void)
{
    enum { CD_AUDIO_DEFAULT_STREAM_SPU_BASE_BYTES = 0x51010 };

    // Retain the repeated first-word stores; these loops never clear whole blocks.
    _cdAudioRepeatFirstWordClear((s32*)&CdAudio_Phase, 2U);
    _cdAudioRepeatFirstWordClear((s32*)&_gCdAudioState, sizeof(_gCdAudioState) / sizeof(s32));

    D_8008277C                      = 0;
    CdAudio_SectorBuffer            = NULL;
    _gCdAudioState.playback.spuBase = CD_AUDIO_DEFAULT_STREAM_SPU_BASE_BYTES;
    spuSetVoiceRange(SPU_VOICE_RANGE_CD_STREAM, CD_AUDIO_RESERVED_VOICE_FIRST, CD_AUDIO_RESERVED_VOICE_COUNT);
    cdStreamReset();
    sndOutputSetStereo(SOUND_OUTPUT_STEREO);
}

/// Returns the player's current driver dispatch code; retained without callers.
///
/// Returns the stored byte unchanged, normally a `CD_AUDIO_DRIVER_*` value.
/// This does not poll the stream or report completion of any operation.
static u8 _cdAudioGetDriver(void)
{
    return _gCdAudioState.playback.driver;
}

void cdAudioPollDriver(void)
{
    if ((_gCdAudioState.playback.baseSector != 0) && (_gCdAudioState.playback.driver != CD_AUDIO_DRIVER_IDLE)) {
        _gCdAudioState.playback.driver = CdAudio_DriveFns[_gCdAudioState.playback.driver & (ARRAY_SIZE(CdAudio_DriveFns) - 1)]();
        cdStreamPollPlayback();
    }
}

/// Sets the absolute disc read base and clears retained read-failure bookkeeping.
///
/// Clears the track-entry base index and settling delay, then drops the header
/// buffer pointer without freeing its allocation or clearing the table aliases.
/// Progress steps and the selected driver are retained. A zero base suppresses
/// driver updates. Always returns 0; this retained entry point has no caller.
static s32 _cdAudioResetReadBase(s32 baseSector)
{
    volatile _CdAudioDriverStatus* driverStatus;

    D_800827E4                         = 0;
    D_80082754                         = 0;
    driverStatus                       = &CdAudio_Ctl;
    driverStatus->settleTicks          = 0;
    driverStatus->failedStep           = 0;
    driverStatus->failureKind          = CD_AUDIO_FAILURE_NONE;
    _gCdAudioState.playback.baseSector = baseSector;
    CdAudio_SectorBuffer               = NULL;
    return 0;
}

/// Replaces the header-sector buffer and requests a read at the stored base sector.
///
/// The nonzero base is an absolute disc sector. Previous header consumers and CD
/// ready callbacks must be quiescent before their buffer is freed. The sound heap
/// must supply a complete header sector; allocation failure is not checked.
/// The player owns the new buffer, and its track table borrows that allocation.
/// Continue driver updates until `CD_AUDIO_HEADER_READ_STEP_DONE` before selecting
/// a header track, or `CD_AUDIO_HEADER_READ_STEP_FAILED` if the read is abandoned.
/// Returns 0 regardless of the drive-mode result.
/// This retained entry point has no caller.
static s32 _cdAudioStartHeaderRead(void)
{
    u8              driveMode;
    _CdAudioHeader* header;

    CdAudio_Phase.headerReadStep = CD_AUDIO_HEADER_READ_STEP_SET_LOCATION;
    CdIntToPos(_gCdAudioState.playback.baseSector, (CdlLOC*)&_gCdAudioState.seekLoc);

    // Replace the owned sector; the slot-table alias is rebound after the read.
    if (CdAudio_SectorBuffer != NULL) {
        sndHeapFree(CdAudio_SectorBuffer);
    }
    header                         = sndHeapAlloc(sizeof(*header));
    CdAudio_SectorBuffer           = (u8*)header;
    CdAudio_SectorEntries          = header->trackEntries;
    _gCdAudioState.playback.driver = CD_AUDIO_DRIVER_READ_HEADER;
    driveMode                      = CdlModeSpeed | CdlModeSize1;
    CdControlB(CdlSetmode, &driveMode, NULL);
    return 0;
}

/// Selects a header track's metadata and requests opening its absolute sector.
///
/// Uses trackNumber's low byte: 0 does nothing; 1..255 name entries 0..254 from
/// the track-entry base. The buffered header and slot table must remain readable,
/// and the entry must precede the slot table; no track count or check is available.
/// Metadata is selected even when playback prevents another opening request.
/// Returns the masked track number, without reporting whether opening started.
/// This retained entry point has no caller.
static s32 _cdAudioOpenHeaderTrack(s32 trackNumber)
{
    enum {
        CD_AUDIO_HEADER_TRACK_NUMBER_MASK = 0xFF,
        CD_AUDIO_HEADER_TRACK_NONE        = 0,
    };
    s32 selectedTrackNumber;

    selectedTrackNumber = trackNumber & CD_AUDIO_HEADER_TRACK_NUMBER_MASK;
    if (selectedTrackNumber != CD_AUDIO_HEADER_TRACK_NONE) {
        _cdAudioOpenTrackAtSector(_gCdAudioState.playback.baseSector + _cdAudioSelectHeaderTrack((trackNumber - 1) & CD_AUDIO_HEADER_TRACK_NUMBER_MASK));
    }
    return selectedTrackNumber;
}

/// Requests playback of an opened stream through the retained private entry point.
///
/// Returns `CD_AUDIO_PLAY_REQUESTED` or `CD_AUDIO_PLAY_NOT_OPEN`; refusal marks
/// playback done and selects the fade stop step. Continue servicing the audio
/// driver after acceptance. This standalone wrapper has no caller.
static s32 _cdAudioPlay(void)
{
    return _cdAudioRequestPlay();
}

/// Selects a table-derived header track's gain and starts a fixed-duration fade-out.
///
/// Returns -1 before opening completes, 1 when any stop step is already selected,
/// or 0 after requesting the fade. The selected sector offset is discarded:
/// this changes the active stream's gain rather than opening another track.
/// The table must contain the indexed entry and its successor, and the header
/// and its slot table must remain readable. This retained routine has no caller;
/// nothing sets its table pointer, and the table's meaning is unproven.
static s32 _cdAudioFadeOutTableTrack(void)
{
    enum {
        CD_AUDIO_TABLE_FADE_NOT_OPEN     = -1,
        CD_AUDIO_TABLE_FADE_STARTED      = 0,
        CD_AUDIO_TABLE_FADE_STOP_PENDING = 1,
        CD_AUDIO_TABLE_TRACK_INDEX_MASK  = 0xFF,
    };
    const _CdAudioTableEntry* tableEntry;
    s32                       trackIndex;
    s32                       result;

    if (CdAudio_Phase.openStep != CD_AUDIO_OPEN_STEP_DONE) {
        return CD_AUDIO_TABLE_FADE_NOT_OPEN;
    }
    if (CdAudio_Phase.stopStep != CD_AUDIO_STOP_STEP_NONE) {
        result = CD_AUDIO_TABLE_FADE_STOP_PENDING;
    } else {
        // The byte difference wraps to a track index; only the selected metadata is used.
        tableEntry = CdAudio_TblEntries + CdAudio_Tbl.field_2;
        trackIndex = (tableEntry[1].field_3 - tableEntry->field_3 - 1) & CD_AUDIO_TABLE_TRACK_INDEX_MASK;
        _cdAudioSelectHeaderTrack(trackIndex);
        _cdAudioStartFadeOut(CD_AUDIO_FADE_OUT_UPDATES);
        result = CD_AUDIO_TABLE_FADE_STARTED;
    }
    return result;
}

s32 cdAudioOpenTrack(s32 startSector, s32 volumeIndex)
{
    enum { CD_AUDIO_VOLUME_INDEX_MASK = 0xFF };
    if (cdStreamIsBusy() != 0) {
        return CD_AUDIO_OPEN_STREAM_BUSY;
    }
    _cdAudioResetTrack(startSector);
    _gCdAudioState.playback.volume = CdAudio_VolumeTable[volumeIndex & CD_AUDIO_VOLUME_INDEX_MASK] << CD_AUDIO_VOLUME_LEVEL_SHIFT;
    sndEvtRequestScriptStop(SOUND_BANK_TYPE_ALL_NON_AMBIENT, SOUND_SCRIPT_STOP_NO_FADE);
    return _cdAudioOpenTrackIfPresent(startSector);
}

/// Resets per-track progress and read failures while retaining the header buffer.
///
/// Keeps a nonzero previous base sector and the header-read step, selects the
/// default stream SPU ring, and records that the opening driver has not run yet.
/// Always returns 0.
static s32 _cdAudioResetTrack(s32 baseSector)
{
    volatile _CdAudioDriverStatus* driverStatus;
    volatile CdAudioProgress*      progress;
    s32                            previousBaseSector;

    D_800827E4         = 0;
    previousBaseSector = _gCdAudioState.playback.baseSector;
    D_80082754         = 0;
    if (previousBaseSector == CD_AUDIO_SECTOR_NONE) {
        _gCdAudioState.playback.baseSector = baseSector;
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

/// Requests opening only for a nonzero absolute sector and returns that sector.
///
/// The opening request can be ignored when playback is active; the returned
/// sector does not report whether an opening was started.
static s32 _cdAudioOpenTrackIfPresent(s32 startSector)
{
    if (startSector != CD_AUDIO_SECTOR_NONE) {
        _cdAudioOpenTrackAtSector(startSector);
    }
    return startSector;
}

s32 cdAudioPlay(void)
{
    return _cdAudioRequestPlay();
}

s32 cdAudioLoadWaves(s32 firstSector)
{
    if (firstSector == CD_AUDIO_SECTOR_NONE) {
        return CD_AUDIO_WAVE_LOAD_NO_SECTOR;
    }
    _cdAudioStartWaveLoad(firstSector, D_80082124);
    return CD_AUDIO_WAVE_LOAD_REQUESTED;
}

/// Sets the absolute disc-sector base for header track offsets and wave reads.
///
/// Set the base before preparing a read's seek location. A zero base suppresses
/// driver updates. This retained setter has no caller.
static void _cdAudioSetBaseSector(s32 baseSector)
{
    _gCdAudioState.playback.baseSector = baseSector;
}

void spuQueueVoiceAttributes(s8 voiceIdx, const SpuVoiceAttr* attributes)
{
    SpuVoiceRef voiceRef;

    spuSetVoiceCallback(voiceIdx, NULL, NULL);
    spuGetVoiceRef(voiceIdx, &voiceRef);
    _spuCopyVoiceAttributes(voiceRef.attr, attributes);
}

void cdStreamAllocVoices(s8* leftVoiceIdx, s8* rightVoiceIdx)
{
    enum {
        CD_STREAM_VOICE_RANGE_REQUEST_COUNT = 3,
        CD_STREAM_VOICE_PRIORITY            = 0xFFFF,
    };

    // Preserve the three-index request; the list has only two declared entries.
    *leftVoiceIdx  = spuAllocVoice(D_80068B28, CD_STREAM_VOICE_RANGE_REQUEST_COUNT, CD_STREAM_VOICE_PRIORITY);
    *rightVoiceIdx = spuAllocVoice(D_80068B28, CD_STREAM_VOICE_RANGE_REQUEST_COUNT, CD_STREAM_VOICE_PRIORITY);
    spuDisableVoiceReverb(*leftVoiceIdx);
    spuDisableVoiceReverb(*rightVoiceIdx);
}

/// Loads a header track's gain, raw flag and slot value, returning its relative sector offset.
///
/// Only trackIndex's low byte is used, added to the player's entry base. That
/// effective index must select a track before the header's slot table; no track
/// count or bounds check is available. Both tables borrow the buffered header
/// and must remain readable. The returned 0..65535 offset is in disc sectors
/// relative to the header's base sector. The flag and slot value are stored
/// only; their meanings are unproven. No live path reads a header in this build.
static s32 _cdAudioSelectHeaderTrack(s32 trackIndex)
{
    enum { CD_AUDIO_HEADER_TRACK_INDEX_MASK = 0xFF };
    union {
        u32 word;
        struct {
            u32 sectorOffset : 16; // Disc sectors relative to the header's base sector
            u32 slotIndex    : 8;  // Index into the header's four-byte slot table
            u32 volumeLevel  : 7;  // Initial gain level, 0..127
            u32 trackFlag    : 1;  // Stored flag; meaning unproven
        } bits;
    } entry;
    const _CdAudioHeaderSlot* slots;

    entry.word                     = CdAudio_SectorEntries[D_80082754 + (trackIndex & CD_AUDIO_HEADER_TRACK_INDEX_MASK)];
    _gCdAudioState.playback.volume = entry.bits.volumeLevel << CD_AUDIO_VOLUME_LEVEL_SHIFT;
    CdAudio_Tbl.trackFlag          = entry.bits.trackFlag;
    slots                          = CdAudio_Tbl.slotTable;
    CdAudio_Tbl.trackSlotValue     = slots[entry.bits.slotIndex].value;
    return entry.bits.sectorOffset;
}

/// Selects the opening driver for an absolute sector when playback is inactive.
///
/// Always returns 0, including when an active play step leaves the request ignored.
static s32 _cdAudioOpenTrackAtSector(s32 startSector)
{
    s32 playStep;

    playStep = CdAudio_Phase.playStep;
    if ((playStep == CD_AUDIO_PLAY_STEP_DONE) || (playStep == CD_AUDIO_PLAY_STEP_NONE)) {
        _gCdAudioState.playback.startSector = startSector;
        _gCdAudioState.playback.driver      = CD_AUDIO_DRIVER_PLAY;
        CdAudio_Phase.openStep              = CD_AUDIO_OPEN_STEP_SILENCE_VOICES;
    }
    return 0;
}

/// Requests playback of an opened stream, or records a refused play request.
///
/// Returns `CD_AUDIO_PLAY_REQUESTED` on acceptance or `CD_AUDIO_PLAY_NOT_OPEN`
/// after marking playback done and selecting the fade stop step. Refusal does
/// not switch drivers.
static s32 _cdAudioRequestPlay(void)
{
    volatile CdAudioProgress* progress;
    s32                       result;

    progress = &CdAudio_Phase;
    if (progress->openStep != CD_AUDIO_OPEN_STEP_DONE) {
        result             = CD_AUDIO_PLAY_NOT_OPEN;
        progress->playStep = CD_AUDIO_PLAY_STEP_DONE;
        progress->stopStep = CD_AUDIO_STOP_STEP_FADE;
    } else {
        result                         = CD_AUDIO_PLAY_REQUESTED;
        progress->stopStep             = CD_AUDIO_STOP_STEP_NONE;
        progress->playStep             = CD_AUDIO_PLAY_STEP_START;
        _gCdAudioState.playback.driver = CD_AUDIO_DRIVER_STOP;
    }
    return result;
}

/// Selects stream fade-out and shutdown over the requested driver-update count.
///
/// With a nonzero starting level, positive updateCount values 1..65535 give a
/// progressing ramp; rounding can extend the fade. Playback is marked done immediately.
static void _cdAudioStartFadeOut(s32 updateCount)
{
    enum { CD_AUDIO_RAMP_LEVEL_MASK = 0xFF };
    LinInterp* ramp;

    ramp = (LinInterp*)&_gCdAudioState.ramp;
    linInterpSetup(ramp, (_gCdAudioState.playback.volume >> CD_AUDIO_VOLUME_LEVEL_SHIFT) & CD_AUDIO_RAMP_LEVEL_MASK, 0, updateCount);
    CdAudio_Phase.playStep         = CD_AUDIO_PLAY_STEP_DONE;
    CdAudio_Phase.stopStep         = CD_AUDIO_STOP_STEP_FADE;
    _gCdAudioState.playback.driver = CD_AUDIO_DRIVER_FADE_OUT;
}

/// Selects the wave loader from an absolute disc sector to an SPU byte address.
///
/// The first wave sector can override spuAddress with its destination selector.
/// The load first waits for the stream to become idle; it does not request
/// stream shutdown itself.
static void _cdAudioStartWaveLoad(s32 firstSector, s32 spuAddress)
{
    _gCdAudioState.playback.baseSector = firstSector;
    CdAudio_Tbl.waveLoadSpuAddress     = spuAddress;
    CdAudio_Phase.waveLoadStep         = CD_AUDIO_WAVE_LOAD_STEP_RELEASE_STREAM;
    _gCdAudioState.playback.driver     = CD_AUDIO_DRIVER_LOAD_WAVES;
}

/// Leaves the CD-audio player idle for idle and reserved dispatch slots.
static s32 _cdAudioIdleDriver(void)
{
    return CD_AUDIO_DRIVER_IDLE;
}

/// Starts an opened stream and waits for its playback and queued drive work to end.
///
/// Returns `CD_AUDIO_DRIVER_STOP`, the playback dispatch slot, while waiting;
/// when the stream becomes idle, returns `CD_AUDIO_DRIVER_FADE_OUT` and enters
/// the stop driver's retained key-off step without requesting a fade.
static s32 _cdAudioPlayDriver(void)
{
    enum { CD_AUDIO_PLAY_STEP_START_UNUSED = 2 }; // Handled like START; never selected by the player.
    s16 nextDriver;

    nextDriver = CD_AUDIO_DRIVER_STOP;
    switch (CdAudio_Phase.playStep) {
        case CD_AUDIO_PLAY_STEP_NONE:
            break;
        case CD_AUDIO_PLAY_STEP_START:
        case CD_AUDIO_PLAY_STEP_START_UNUSED:
            cdStreamBeginPlayback();
            CdAudio_Phase.playStep = CD_AUDIO_PLAY_STEP_WAIT_END;
            break;
        case CD_AUDIO_PLAY_STEP_WAIT_END:
            if (cdStreamIsBusy() == 0) {
                nextDriver             = CD_AUDIO_DRIVER_FADE_OUT;
                CdAudio_Phase.playStep = CD_AUDIO_PLAY_STEP_DONE;
                CdAudio_Phase.stopStep = CD_AUDIO_STOP_STEP_RELEASE_VOICES;
            }
            break;
        case CD_AUDIO_PLAY_STEP_DONE:
            break;
    }
    return nextDriver;
}

/// Takes the first ready header sector, recording its location error if needed.
///
/// The result buffer is unused. A wrong-sector payload is still copied and
/// reported as arrived; non-data-ready interrupts record an error without
/// consuming data. Later interrupts are ignored once a payload has arrived.
static void _cdAudioHeaderSectorReadyCallback(u8 interruptStatus, u8* unusedResult)
{
    s32                        readyStatus;
    s32                        sectorPosition;
    FsSector*                  sector;
    volatile _CdAudioPlayback* playback;

    if (D_80082770 != 0) {
        return;
    }

    readyStatus = interruptStatus;
    if (readyStatus == CdlDataReady) {
        sector = &Fs_CdSector;
        CD_AUDIO_READ_HEADER_SECTOR_POSITION(sector, playback, sectorPosition);
        if (playback->baseSector != sectorPosition) {
            CdAudio_Ctl.headerReadError = CD_AUDIO_HEADER_READ_ERROR_WRONG_SECTOR;
        }
        CdGetSector(CdAudio_SectorBuffer, sizeof(_CdAudioHeader) / sizeof(u32));
        D_80082770 = readyStatus;
    } else {
        CdAudio_Ctl.headerReadError = CD_AUDIO_HEADER_READ_ERROR_NOT_READY;
    }
}

/// Reports that the stream's opening read finished or was abandoned.
///
/// The callback argument is 1 for an opened stream and 0 for an abandoned read;
/// both raise the same report so the track-opening driver can leave its wait.
static void _cdAudioStreamOpeningDoneCallback(s32 unusedOpened)
{
    _gCdAudioState.playback.startReported = 1;
}
