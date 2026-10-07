#ifndef GAMEPLAY_PAD_INPUT_H
#define GAMEPLAY_PAD_INPUT_H

#include "types.h"

#include "main/pad.h"

extern s16 Gp_MenuLockDelay;

/// Suppression commands; the alternate values perform the same operations.
enum {
    PAD_INPUT_SUPPRESSION_CLEAR        = 0,
    PAD_INPUT_SUPPRESSION_SET          = 1,
    PAD_INPUT_SUPPRESSION_CLEAR_ALIAS  = 2,
    PAD_INPUT_SUPPRESSION_SET_AND_HOLD = 3,
    PAD_INPUT_SUPPRESSION_KEEP         = 4,
    PAD_INPUT_SUPPRESSION_SET_ALIAS    = 5,
};

/// Logical buttons commonly suppressed during scripted scenes.
///
/// These masks apply after layout remapping. L3 and R3 are excluded.
enum {
    PAD_INPUT_SUPPRESS_MENU             = PAD_BUTTON_SELECT | PAD_BUTTON_START,
    PAD_INPUT_SUPPRESS_ACTIONS_AND_MENU = PAD_INPUT_SUPPRESS_MENU |
                                          PAD_BUTTON_L2 | PAD_BUTTON_R2 | PAD_BUTTON_L1 | PAD_BUTTON_R1 |
                                          PAD_BUTTON_TRIANGLE | PAD_BUTTON_CIRCLE | PAD_BUTTON_CROSS | PAD_BUTTON_SQUARE,
    PAD_INPUT_SUPPRESS_GAMEPLAY = PAD_INPUT_SUPPRESS_ACTIONS_AND_MENU |
                                  PAD_BUTTON_UP | PAD_BUTTON_RIGHT | PAD_BUTTON_DOWN | PAD_BUTTON_LEFT,
};

/// Changes the logical-button suppression mask and automatic menu-lock policy.
///
/// SET (1/5) adds bits and resumes automatic menu-lock updates; CLEAR (0/2) removes
/// bits and resumes them. SET_AND_HOLD (3) adds bits and freezes automatic lock
/// updates until a SET or CLEAR request. KEEP (4) and other byte values do nothing.
/// Only the low 16 bits of `buttonMask` affect suppression. Requests update the
/// shared mask without reference counting; a clear removes bits set by any caller.
/// Filtering and display menu-hold changes occur on the next `padInputUpdate`,
/// provided the player task exists. Resident pad samples remain available.
void padInputChangeSuppression(u8 command, s32 buttonMask);

#endif // GAMEPLAY_PAD_INPUT_H
