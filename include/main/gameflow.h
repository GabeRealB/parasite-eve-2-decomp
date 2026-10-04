#ifndef MAIN_GAMEFLOW_H
#define MAIN_GAMEFLOW_H

#include "types.h"

#include "main/display_types.h"

/// Queues a blended 320x240 colour overlay for the current frame.
///
/// The channels are byte intensities; wider arguments wrap to their low byte.
/// The low two bits of `blendMode` select a `GPU_BLEND_*` mode: subtracting
/// darkens towards black, adding brightens towards white. The rectangle uses
/// centred 320x240 coordinates and cancels the applied vertical screen shake.
/// Callers advance the fade themselves and must queue it again each frame.
///
/// The current ordering table must contain foreground tag -16, and the frame
/// packet arena must have `sizeof(TILE) + sizeof(DR_TPAGE)` writable bytes.
/// Both packets borrow that arena until GPU drawing completes. The blend
/// command enables dithering, disables drawing into the displayed area and
/// remains active until another draw-mode command replaces it.
void fadeDrawOverlay(u8 red, u8 green, u8 blue, s32 blendMode);

#endif // MAIN_GAMEFLOW_H
