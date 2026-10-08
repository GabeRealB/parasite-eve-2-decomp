#ifndef MAIN_PAD_H
#define MAIN_PAD_H

#include <psyq/sys/types.h>
#include <psyq/libetc.h>

#include "types.h"

#include "main/game_debug_types.h"
#include "main/pad_types.h"

/// Persistent processed input, controller setup and vibration state for ports 0 and 1.
///
/// `padInit` resets both entries; VSync polling and main-loop button updates
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

/// Tests the stored Start-button press on controller port 0.
///
/// Returns a signed 32-bit 1 or 0 using `PAD_BUTTON_QUERY_PRESSED`.
/// Reads input without polling or consuming it. UI repeat affects only D-pad
/// buttons, so holding Start does not generate repeated presses.
s32 padIsStartPressed(void);

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

/// Motor drive values for timed vibration requests.
enum {
    PAD_VIBRATION_BINARY_ON     = 1,
    PAD_VIBRATION_INTENSITY_MAX = 255,
};

/// Adds a timed vibration contribution to one controller motor's request bank.
///
/// `port` must be 0 or 1; `motorBank` is `PAD_VIBRATION_MOTOR_BINARY` (0) or
/// `PAD_VIBRATION_MOTOR_VARIABLE` (1). Intensity is stored as its low byte:
/// zero contributes no drive, nonzero drives the binary motor, and 0..255
/// sets variable-motor strength. Requests mix by binary OR or variable maximum.
/// Demo scenes suppress the post without changing requests or the shared cursor.
///
/// Searches eight slots starting at the cursor shared by both motor banks.
/// Uses the first inactive slot, or replaces the last examined slot if all are
/// active. The cursor advances past an available slot; a full-bank scan wraps
/// to its starting cursor and then advances once more, independently of the
/// replaced slot. The stored request belongs to resident controller state.
///
/// Duration uses the signed low halfword of `durationUnits`, doubled and stored
/// as a signed halfword without clamping. Each unit normally spans two serviced
/// controller polls (VSyncs); skipped polls pause the countdown, and the expiry
/// poll still contributes. The countdown wraps at 16 bits: a stored zero lasts
/// 65536 serviced polls rather than cancelling. Posting does not poll or send
/// an actuator command; polling currently services only port 0.
void padPostVibrationRequest(s32 port, s32 motorBank, s32 intensity, s32 durationUnits);

/// Starts or renews a controller port's 61-update input block.
///
/// `port` must be 0 or 1. Replaces the countdown without clearing stored button
/// masks immediately. Blocked input updates clear held, pressed and released
/// masks; the expiry update samples raw held buttons without generating edges.
/// The countdown advances during main-loop input updates, not VSync polls;
/// currently only port 0 receives those updates. `padReadRawButtons` remains
/// available during the block.
void padStartInputBlock(s32 port);

/// Clears the pending input-block countdown for a controller port.
///
/// `port` must be 0 or 1. Stored button masks remain available until the next
/// main-loop input update resumes sampling and computes edges from the stored
/// held mask. The countdown counts input updates, not VSync vibration polls.
void padClearInputBlock(s32 port);

/// Returns the raw held buttons as active-high Psy-Q bits, even during an input block.
///
/// `port` must be 0 or 1. Returns a zero-extended 16-bit mask in a signed 32-bit
/// value. Reads the latest receive-buffer bytes without polling or consuming
/// them; it adds no stick directions, replay override or pressed/released edges.
/// Does not validate the receive-buffer status. Resident polling currently
/// services port 0 and supplies all-released bytes while that pad is unusable;
/// port 1 can retain response bytes from an earlier successful exchange.
s32 padReadRawButtons(s32 port);

/// Clears every stored vibration request for both motors of one controller port.
///
/// `port` must be 0 or 1. Resets the shared slot cursor to zero, including during
/// demo scenes. The actuator command buffer is refreshed on the next serviced
/// poll; this call does not poll or send a command.
void padClearVibrationRequests(s32 port);

#endif // MAIN_PAD_H
