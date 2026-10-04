#ifndef MAIN_PAD_H
#define MAIN_PAD_H

#include <psyq/sys/types.h>
#include <psyq/libetc.h>

#include "types.h"

#include "main/game_debug_types.h"
#include "main/pad_types.h"

/// Persistent processed input, controller setup and vibration state for ports 0 and 1.
///
/// `Pad_Init` resets both entries; VSync polling and main-loop button updates
/// currently service only port 0. Port-taking pad APIs require an index in 0..1.
/// These records are separate from libpad's raw receive buffers. Their embedded
/// actuator command buffers stay at fixed addresses while communication runs,
/// including across overlay loads; consumers may clear pressed-button bits.
extern PadState gPadStates[PAD_PORT_COUNT];

extern GameDebugState* Pad_RemapState;

// Button masks checked with padCheckButtons: confirm (0x40), cancel
// (0xA0) and menu-open (0x900). Gp_PadSuppressMask masks out 0x900
// exactly while the in-game menu is locked.
extern s32 Pad_MaskConfirm;

extern s32 Pad_MaskCancel;

extern s32 Pad_MaskMenu;

/// Returns padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, PAD_BUTTON_START); non-void so callers can branch on v0.
s32 Pad_CheckFlag800(void);

/// Active-high button bits in the processed pad sample, using Psy-Q bit order.
///
/// D-pad bits also include directions synthesized from the left analog stick.
enum {
    PAD_BUTTON_L2       = PADL2,
    PAD_BUTTON_R2       = PADR2,
    PAD_BUTTON_L1       = PADL1,
    PAD_BUTTON_R1       = PADR1,
    PAD_BUTTON_TRIANGLE = PADRup,
    PAD_BUTTON_CIRCLE   = PADRright,
    PAD_BUTTON_CROSS    = PADRdown,
    PAD_BUTTON_SQUARE   = PADRleft,
    PAD_BUTTON_SELECT   = PADselect,
    PAD_BUTTON_L3       = PADi,
    PAD_BUTTON_R3       = PADj,
    PAD_BUTTON_START    = PADstart,
    PAD_BUTTON_UP       = PADLup,
    PAD_BUTTON_RIGHT    = PADLright,
    PAD_BUTTON_DOWN     = PADLdown,
    PAD_BUTTON_LEFT     = PADLleft,
};

/// Button sample and mask predicate selected by `padCheckButtons`.
enum {
    PAD_BUTTON_QUERY_HELD_ANY = 0,
    PAD_BUTTON_QUERY_PRESSED  = 1, // Newly pressed bits plus UI D-pad repeat
    PAD_BUTTON_QUERY_HELD_ALL = 2,
    PAD_BUTTON_QUERY_RELEASED = 3,
};

/// Returns 1 when the selected processed button sample matches `mask`, else 0.
///
/// `port` must be 0 or 1. `mode` selects a `PAD_BUTTON_QUERY_*` predicate;
/// values other than 1, 2 and 3 use the held-any predicate. Held samples include
/// synthesized stick directions; pressed samples also include UI D-pad repeat.
/// The query reads the stored sample without consuming or polling input.
///
/// The 16-bit sample is zero-extended before testing the full signed 32-bit
/// `mask`. Held-all requires every mask bit; the other predicates require any
/// bit. A zero mask returns 1 only for held-all.
s32 padCheckButtons(s32 port, s32 mode, s32 mask);

void Pad_PostEvent(s32 port, s32 bank, s32 arg2, s32 arg3);

void Pad_SetCooldown(s32 port);

/// Clears the pending input-block countdown for a controller port.
///
/// `port` must be 0 or 1. Stored button masks remain available until the next
/// main-loop input update resumes sampling and computes edges from the stored
/// held mask. The countdown counts input updates, not VSync vibration polls.
void padClearInputBlock(s32 port);

s32 Pad_ReadButtonsInv(s32 port);

void Pad_ClearEvents(s32 port);

#endif // MAIN_PAD_H
