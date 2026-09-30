#ifndef MAIN_GAMEMAIN_H
#define MAIN_GAMEMAIN_H

#include "types.h"

extern volatile s32 GameMain_HaltFlags;

u32 GameMain_GetResetCount(void);

/// Frame-pacing selectors; stored animation steps are respectively 1, 2 and 3.
enum {
    DISPLAY_TIMING_EVERY_VBLANK  = 0,
    DISPLAY_TIMING_TWO_VBLANKS   = 1,
    DISPLAY_TIMING_THREE_VBLANKS = 2,
};

/// Select VSync pacing and the nominal 60-Hz animation step.
///
/// `timingMode` is a DISPLAY_TIMING_* value. Play-clock accumulation is separate.
void GameMain_SetFrameTiming(s32 timingMode);

#endif // MAIN_GAMEMAIN_H
