#ifndef MAIN_PAD_H
#define MAIN_PAD_H

#include "types.h"

#include "main/pad_types.h"

/// Persistent processed input, controller setup and vibration state for ports 0 and 1.
///
/// `Pad_Init` resets both entries; VSync polling and main-loop button updates
/// currently service only port 0. Port-taking pad APIs require an index in 0..1.
/// These records are separate from libpad's raw receive buffers. Their embedded
/// actuator command buffers stay at fixed addresses while communication runs,
/// including across overlay loads; consumers may clear pressed-button bits.
extern PadState gPadStates[PAD_PORT_COUNT];

extern PadRemapState* Pad_RemapState;

// Button masks checked with Pad_CheckButtons: confirm (0x40), cancel
// (0xA0) and menu-open (0x900). Gp_PadSuppressMask masks out 0x900
// exactly while the in-game menu is locked.
extern s32 Pad_MaskConfirm;

extern s32 Pad_MaskCancel;

extern s32 Pad_MaskMenu;

/// Returns Pad_CheckButtons(0, 1, 0x800); non-void so callers can branch on v0.
s32 Pad_CheckFlag800(void);

s32 Pad_CheckButtons(s32 port, s32 mode, s32 mask);

void Pad_PostEvent(s32 port, s32 bank, s32 arg2, s32 arg3);

void Pad_SetCooldown(s32 port);

void Pad_ClearCooldown(s32 port);

s32 Pad_ReadButtonsInv(s32 port);

void Pad_ClearEvents(s32 port);

#endif // MAIN_PAD_H
