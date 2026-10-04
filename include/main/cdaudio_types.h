#ifndef MAIN_CDAUDIO_TYPES_H
#define MAIN_CDAUDIO_TYPES_H

#include "common.h"

/// `CdAudioProgress::openStep` values: opening a track's stream.
enum {
    CD_AUDIO_OPEN_STEP_NONE           = 0, // no track is being opened
    CD_AUDIO_OPEN_STEP_START_READ     = 1, // start the stream's opening read, or give the track up if a cancel is pending
    CD_AUDIO_OPEN_STEP_WAIT_READ      = 2, // wait for the stream to report its opening read
    CD_AUDIO_OPEN_STEP_DONE           = 3, // the track is open and ready to play, or its opening was cancelled
    CD_AUDIO_OPEN_STEP_SILENCE_VOICES = 4, // entry step: key off the stream voices until both are silent
};

/// `CdAudioProgress::playStep` values: playing an open track through.
///
/// A value of 2 is handled as `CD_AUDIO_PLAY_STEP_START`; nothing stores it.
enum {
    CD_AUDIO_PLAY_STEP_NONE     = 0, // nothing has played since the track was set up
    CD_AUDIO_PLAY_STEP_START    = 1, // set the stream running
    CD_AUDIO_PLAY_STEP_WAIT_END = 3, // wait for the stream to go idle, then release its voices through the stop steps
    CD_AUDIO_PLAY_STEP_DONE     = 4, // playback is over: it ran out, was refused, or a stop took its place
};

/// `CdAudioProgress::stopStep` values: silencing the stream and bringing the
/// player to rest.
enum {
    CD_AUDIO_STOP_STEP_NONE           = 0, // no stop since the track was set up
    CD_AUDIO_STOP_STEP_FADE           = 1, // wind the volume down to silence, then stop the stream
    CD_AUDIO_STOP_STEP_RELEASE_VOICES = 2, // key off the stream's voices
    CD_AUDIO_STOP_STEP_WAIT_IDLE      = 3, // wait for the stream to go idle
    CD_AUDIO_STOP_STEP_DONE           = 4, // the player is at rest; also reported when a cancel finds nothing playing and when a wave load completes
};

/// `CdAudioProgress::waveLoadStep` values: reading wave data off the disc into
/// the SPU.
///
/// A failed or timed-out step restarts the load from the drive mode.
enum {
    CD_AUDIO_WAVE_LOAD_STEP_NONE              = 0,  // no load requested since the track was set up
    CD_AUDIO_WAVE_LOAD_STEP_RELEASE_STREAM    = 1,  // entry step: key off the stream's voices, wait for it to go idle, then set the drive mode
    CD_AUDIO_WAVE_LOAD_STEP_WAIT_SET_MODE     = 2,  // wait for the drive to take the mode
    CD_AUDIO_WAVE_LOAD_STEP_SETTLE            = 3,  // let a few ticks pass
    CD_AUDIO_WAVE_LOAD_STEP_SET_LOCATION      = 4,  // point the drive at the first wave sector
    CD_AUDIO_WAVE_LOAD_STEP_WAIT_SET_LOCATION = 5,  // wait for the drive to take the location
    CD_AUDIO_WAVE_LOAD_STEP_START_READ        = 6,  // start reading sectors into the SPU upload
    CD_AUDIO_WAVE_LOAD_STEP_WAIT_READ         = 7,  // wait for the drive to accept the read
    CD_AUDIO_WAVE_LOAD_STEP_WAIT_UPLOAD       = 8,  // wait for the last sector to be uploaded
    CD_AUDIO_WAVE_LOAD_STEP_WAIT_PAUSE        = 9,  // wait for the drive to pause
    CD_AUDIO_WAVE_LOAD_STEP_DONE              = 10, // the wave data is loaded and the drive paused
    CD_AUDIO_WAVE_LOAD_STEP_PAUSE             = 11, // end the read and pause the drive; a cancel sends a running load here
};

/// How far the CD audio player has got with each of its operations.
///
/// Opening a track, playing it, stopping, reading a header sector and loading
/// wave data each keep a step of their own, advanced once a tick while that
/// operation is the one the player is running. Code outside the player waits
/// for an operation by polling its step.
///
/// Initialisation zeroes only the first four steps: its clear loop stores to
/// the block's first word on every pass and never advances. Setting up a track
/// zeroes everything but `headerReadStep`.
typedef struct {
    u8 openStep;        // `CD_AUDIO_OPEN_STEP_*`
    u8 playStep;        // `CD_AUDIO_PLAY_STEP_*`
    u8 stopStep;        // `CD_AUDIO_STOP_STEP_*`
    u8 headerReadStep;  // how far the header sector read has got; its values are private to the player
    u8 waveLoadStep;    // `CD_AUDIO_WAVE_LOAD_STEP_*`
    u8 cancelRequested; // 1 from a cancel request until the next track is set up: a track still opening then ends up at rest instead of ready to play
} CdAudioProgress;
STATIC_ASSERT_SIZEOF(CdAudioProgress, 0x6);

#endif // MAIN_CDAUDIO_TYPES_H
