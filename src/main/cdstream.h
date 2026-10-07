#ifndef MAIN_PRIVATE_CDSTREAM_H
#define MAIN_PRIVATE_CDSTREAM_H

#include "common.h"

/// `CdStreamParams::voiceL` and `voiceR` value for a channel with no SPU voice yet.
#define CD_STREAM_VOICE_NONE (-1)

/// Describes one CD-to-SPU MTS stream to `CdStream_Start`.
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

// CD → SPU MTS stream
void CdStream_Reset(void);

/// Begins playback of an opened stream from playhead zero on the next driver ticks.
///
/// The opening read must have finished. Resets the initial chunk delay and
/// registers the SPU ring-boundary callback while leaving the interrupt disabled;
/// chunk reads arm it when needed. Voice allocation and key-on happen in the driver.
void cdStreamBeginPlayback(void);

/// Sets both streaming voices' gains, respecting mono/stereo output.
void CdStream_SetVolume(s16 volume);

/// Returns one while playback, a queued disc read or drive reinitialization is outstanding.
///
/// An opened stream waiting to begin playback is idle once its queues drain.
/// Cancellation remains busy until the CD-ready queue has retired its job.
s32 cdStreamIsBusy(void);

/// Selects mono or stereo mixing for subsequent stream gain updates.
///
/// A nonzero low byte of `enabled` selects mono; a zero low byte selects stereo.
/// Stream setup and `CdStream_SetVolume` apply the selection: mono sends each
/// channel to both outputs at 181/256 of its gain; stereo sends the left channel
/// to the left output and the right channel to the right output at full gain.
void cdStreamSetMono(s32 enabled);

void CdStream_Start(CdStreamParams* params);

void CdStream_Stop(void);

void CdStream_Drive(void);

#endif // MAIN_PRIVATE_CDSTREAM_H
