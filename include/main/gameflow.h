#ifndef MAIN_GAMEFLOW_H
#define MAIN_GAMEFLOW_H

#include "types.h"

#include "main/display_types.h"

/// Draws a full-screen colour tile over the frame, blended with `mode`, a
/// `GPU_BLEND_*` semitransparency mode (`GPU_BLEND_SUBTRACT` darkens towards
/// black, `GPU_BLEND_ADD` brightens towards white).
void Fade_DrawOverlay(s32 r, s32 g, s32 b, s32 mode);

#endif // MAIN_GAMEFLOW_H
