#ifndef MAIN_PRIVATE_CDAUDIO_H
#define MAIN_PRIVATE_CDAUDIO_H

#include "types.h"

extern u8 D_80068AF0[];

extern u16 Spu_SemitonePitchTable[];

extern u16 Spu_FinePitchTable[];

extern u16 Snd_PanGainTable[];

extern u16 Snd_VelocityGainTable[];

/// Results of requesting CD-audio cancellation.
enum {
    CD_AUDIO_CANCEL_OPEN_PENDING = -3, // Track setup has not reached its opening driver; cancel remains requested
    CD_AUDIO_CANCEL_NO_FADE      = -2, // No playback to fade, or a wave load was redirected to its pause step
    CD_AUDIO_CANCEL_OPENING      = -1, // Opening is still in progress; its driver observes the cancel request
    CD_AUDIO_CANCEL_FADE_STARTED = 0,  // Fade-out and stream shutdown were started
    CD_AUDIO_CANCEL_STOP_PENDING = 1,  // A stop step was already selected
};

/// Requests cancellation of track opening, playback or a wave upload.
///
/// Returns a `CD_AUDIO_CANCEL_*` result. The request remains latched until
/// another track is set up; continue servicing the audio driver and poll
/// `CdAudio_Phase.stopStep` for `CD_AUDIO_STOP_STEP_DONE` before reusing the stream.
s32 cdAudioCancel(void);

void CdAudio_Init(void);

void CdAudio_Tick(void);

#endif // MAIN_PRIVATE_CDAUDIO_H
