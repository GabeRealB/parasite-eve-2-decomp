#ifndef MAIN_PRIVATE_CDSTREAM_H
#define MAIN_PRIVATE_CDSTREAM_H

#include "common.h"

/// `CdStreamParams::voiceL` and `voiceR` value for a channel with no SPU voice yet.
#define CD_STREAM_VOICE_NONE (-1)

/// Describes one CD-to-SPU MTS stream to `cdStreamOpen`.
///
/// The call copies every field into the live stream and keeps no reference to
/// the block. The stream reads each disc sector into `sectorBuf` and transfers
/// its ADPCM into an SPU ring at `spuBase`, one half per chunk and one ring
/// per channel. A null callback is skipped.
typedef struct {
    s32   startSector;                   // absolute disc sector of the first MTS chunk
    s32   spuBase;                       // SPU address of the left ring; the right ring follows it
    void* sectorBuf;                     // caller's buffer for one 2048-byte sector, opening with the MTS header
    void  (*doneCb)(s32 opened);         // 1 when the opening read finishes, 0 when a read is abandoned
    void  (*startCb)(s32 voiceMask);     // the stream's two voice bits, when they are keyed on
    void  (*voiceFreeCb)(s32 voiceMask); // the stream's two voice bits, when they are keyed off
    s16   volume;                        // SPU gain of each channel on its own side; a mono mix puts 181/256 of it on both sides
    s8    voiceL;                        // left SPU voice until key-on allocates the pair, `CD_STREAM_VOICE_NONE` for none
    s8    voiceR;                        // right SPU voice until key-on allocates the pair, `CD_STREAM_VOICE_NONE` for none
    s8    channelCount;                  // MTS channels interleaved per chunk, until the first header supplies the count
} CdStreamParams;
STATIC_ASSERT_SIZEOF(CdStreamParams, 0x20);

/// Clears the stream and ready queue and starts the SPU-transfer timeout counter.
///
/// Call during audio initialization before starting reads or playback. Clearing
/// the state does not cancel hardware operations, remove callbacks or free voices.
void cdStreamReset(void);

/// Acquires the stream's left and right SPU voices and disables their reverb.
///
/// Both outputs must point to distinct writable signed voice-index bytes. Allocation
/// prefers the CD-stream range, then the sound-script range, at priority 65535.
/// The retained request scans three indices from a two-entry list if neither
/// range supplies a free voice; that exhaustion path reads beyond the list.
/// Allocation failure is stored as -1 and is still passed to reverb control.
/// Allocation can notify a previous owner and leaves its callback registered;
/// queue each channel's attributes before the next audio tick to clear it.
void cdStreamAllocVoices(s8* leftVoiceIdx, s8* rightVoiceIdx);

/// Begins playback of an opened stream from playhead zero on the next driver ticks.
///
/// The opening read must have finished. Resets the initial chunk delay and
/// registers the SPU ring-boundary callback while leaving the interrupt disabled;
/// chunk reads arm it when needed. Voice allocation and key-on happen in the driver.
void cdStreamBeginPlayback(void);

/// Sets both streaming voices' gains, respecting mono/stereo output.
///
/// `volume` is a signed SPU gain, normally 0..0x3FFF. Mono writes 181/256 of
/// it to both outputs of both channels, narrowing the result to s16; stereo
/// writes it only to each channel's own output. Engaged playback marks the
/// volume attributes for copying by the next driver poll; other pending edits survive.
void cdStreamSetVolume(s16 volume);

/// Returns one while playback, a queued disc read or drive reinitialization is outstanding.
///
/// An opened stream waiting to begin playback is idle once its queues drain.
/// Cancellation remains busy until the CD-ready queue has retired its job.
s32 cdStreamIsBusy(void);

/// Selects mono or stereo mixing for subsequent stream gain updates.
///
/// A nonzero low byte of `enabled` selects mono; a zero low byte selects stereo.
/// Stream setup and `cdStreamSetVolume` apply the selection: mono sends each
/// channel to both outputs at 181/256 of its gain; stereo sends the left channel
/// to the left output and the right channel to the right output at full gain.
void cdStreamSetMono(s32 enabled);

/// Configures the stream and queues its opening chunk read, leaving playback inactive.
///
/// The setup block is borrowed only for this call. Its sector buffer and SPU
/// rings must remain available until reads, transfers and playback have ended.
/// Header-sector transfers read 2048 bytes starting after the 16-byte header,
/// so the buffer must provide at least 2064 readable bytes. Each channel's ring
/// has two halves, initially 0x2770 bytes each. The first header selects 0x2770
/// for a five-sector period, otherwise 0x4ED0; the right ring follows the left
/// with a 64-byte gap. Reserve both rings for that final size.
/// The ready queue owns a copy of the opening job. A full queue leaves no job
/// queued and gives no completion notification. On success, doneCb receives 1;
/// call `cdStreamBeginPlayback` afterwards to start playback and allocate voices.
///
/// Reopening an active stream retains the original replacement order: key-off
/// uses the supplied voices and voiceFreeCb, rather than the previous setup.
void cdStreamOpen(const CdStreamParams* params);

/// Requests stream shutdown, or cancels an opening read when playback is inactive.
///
/// Active playback keys off its voices and cancels its read on the next driver
/// poll. Cancellation handlers may require further polls; wait for
/// `cdStreamIsBusy` to clear before reusing the sector buffer or SPU rings.
void cdStreamStop(void);

void CdStream_Drive(void);

#endif // MAIN_PRIVATE_CDSTREAM_H
