#ifndef MAIN_CDAUDIO_TYPES_H
#define MAIN_CDAUDIO_TYPES_H

#include "common.h"

/// CD audio player phase machine (CdAudio_Phase). Driven by switches in 46FE4.c.
typedef struct _CdAudioPhase {
    /* 0x0 */ u8 field_0; // primary phase
    /* 0x1 */ u8 field_1; // sub-phase
    /* 0x2 */ u8 field_2; // sub-phase / gate
    /* 0x3 */ u8 field_3; // stream sub-phase
    /* 0x4 */ u8 field_4; // seek / load sub-phase
    /* 0x5 */ u8 field_5; // control / abort flag
} CdAudioPhase;
STATIC_ASSERT_SIZEOF(CdAudioPhase, 0x6);

#endif // MAIN_CDAUDIO_TYPES_H
