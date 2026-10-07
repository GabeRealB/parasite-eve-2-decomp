#ifndef MAIN_CDAUDIO_H
#define MAIN_CDAUDIO_H

#include "types.h"

#include "main/cdaudio_types.h"

extern volatile CdAudioProgress CdAudio_Phase;

/// Results of a CD-audio playback request.
enum {
    CD_AUDIO_PLAY_NOT_OPEN  = -1, // Opening has not completed; mark playback done and select the fade stop step
    CD_AUDIO_PLAY_REQUESTED = 0,  // Start the opened stream on subsequent driver ticks
};

/// A zero absolute disc sector represents an absent track or wave upload.
enum { CD_AUDIO_SECTOR_NONE = 0 };

/// Refusal result of a track-opening request while the stream is busy.
enum { CD_AUDIO_OPEN_STREAM_BUSY = -1 };

/// Results of requesting a CD-audio wave upload.
enum {
    CD_AUDIO_WAVE_LOAD_NO_SECTOR = -1,
    CD_AUDIO_WAVE_LOAD_REQUESTED = 0,
};

/// Sets up a track's stream at an absolute disc sector without starting playback.
///
/// Returns `CD_AUDIO_OPEN_STREAM_BUSY` without changing the player while the
/// stream is busy; otherwise returns startSector. A zero sector resets the track
/// and requests sound script shutdown, but starts no opening read. volumeIndex's low byte must
/// index the 112-entry initial-gain table (0..111); no bounds check is performed.
/// Continue servicing the driver until `CdAudio_Phase.openStep` is
/// `CD_AUDIO_OPEN_STEP_DONE`, then call `cdAudioPlay`.
s32 cdAudioOpenTrack(s32 startSector, s32 volumeIndex);

/// Requests an asynchronous SPU wave upload beginning at an absolute disc sector.
///
/// Returns `CD_AUDIO_WAVE_LOAD_NO_SECTOR` for a zero sector, otherwise
/// `CD_AUDIO_WAVE_LOAD_REQUESTED`. The upload uses the sound loader's
/// saved SPU destination unless the first sector selects a destination-table entry.
/// Keep the shared sector buffer and sound loader available; continue servicing
/// the audio driver until `CdAudio_Phase.waveLoadStep` is
/// `CD_AUDIO_WAVE_LOAD_STEP_DONE`. A failed read restarts the upload.
s32 cdAudioLoadWaves(s32 firstSector);

/// Requests playback of the stream opened by `cdAudioOpenTrack`.
///
/// Returns `CD_AUDIO_PLAY_REQUESTED` when opening has completed, or
/// `CD_AUDIO_PLAY_NOT_OPEN` after marking playback done and selecting the fade
/// stop step. Continue servicing the driver; playback completion is reported by
/// `CdAudio_Phase.playStep == CD_AUDIO_PLAY_STEP_DONE`.
s32 cdAudioPlay(void);

#endif // MAIN_CDAUDIO_H
