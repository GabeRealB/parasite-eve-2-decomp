#ifndef GAMEPLAY_ACTION_PROMPT_H
#define GAMEPLAY_ACTION_PROMPT_H

#include "common.h"

/// Signed screen position of an action-prompt cursor, in pixels.
///
/// The origin is the screen center and Y increases downward, the same space as
/// a hotspot rectangle: a full-screen hotspot covers [-160, -120, 320, 240].
/// Each frame the cursor task stores the pixel coordinate of its 1/512-pixel
/// position here; those writes stay within X [-160, 159] and Y [-110, 110].
/// The two shorts occupy one word, so a button slot can latch the position and
/// tell whether the cursor has moved.
typedef struct {
    s16 x; // Horizontal pixels from the screen center
    s16 y; // Vertical pixels from the screen center, increasing downward
} ActionPromptCursorPos;
STATIC_ASSERT_SIZEOF(ActionPromptCursorPos, 4);

/// Screen position of an action-prompt cursor, as pixels or as one word.
///
/// `xy` is the pixel position in the same center-origin space as
/// `ActionPromptCursorPos`. `packed` is those two shorts in one word. A button
/// slot copies that word and compares it on the next press to tell whether the
/// cursor has moved. Storing either member replaces the other.
typedef union {
    ActionPromptCursorPos xy;     // Horizontal and vertical pixels from the screen center
    s32                   packed; // Both pixels in one word
} ActionPromptScreen;
STATIC_ASSERT_SIZEOF(ActionPromptScreen, 4);

/// One of the two button slots at the tail of `ActionPrompt`. `state` is the
/// press classification the cursor task writes each frame (0 none, 1 held,
/// 2 pressed, 3 released, 4 double-press), `heldFrames` counts the frames since
/// the slot was last armed and `lastPos` latches the cursor position of the
/// previous press so a double-press only registers when the cursor has not
/// moved. Slot 0 watches the confirm mask (0x40) and slot 1 the cancel mask
/// (0xA0).
typedef struct RoomActionPromptButton {
    /* 0x0 */ u16 state;
    /* 0x2 */ u16 heldFrames;
    /* 0x4 */ s32 lastPos;
} RoomActionPromptButton;
STATIC_ASSERT_SIZEOF(RoomActionPromptButton, 0x8);

/// Motion multiplier stored in `ActionPrompt::cursorSpeed`.
///
/// Stopped holds the cursor. Aiming is the speed a hotspot scan stores while
/// the player can move the cursor. Reset is the speed stored when the prompt
/// is reset, before a scan slows it.
#define ACTION_PROMPT_SPEED_STOPPED 0
#define ACTION_PROMPT_SPEED_AIM     0x80
#define ACTION_PROMPT_SPEED_RESET   0x100

/// Cursor sprite stored in `ActionPrompt::mode`.
///
/// Hidden draws nothing. Idle is the cursor while it is not on a hotspot.
/// Hotspot is the cursor while the point is inside a hotspot. Accepting a
/// hotspot or leaving the scan stores hidden.
#define ACTION_PROMPT_MODE_HIDDEN  0
#define ACTION_PROMPT_MODE_IDLE    1
#define ACTION_PROMPT_MODE_HOTSPOT 2

/// Frames stored in `doublePressWindow` when the prompt is reset.
#define ACTION_PROMPT_DOUBLE_PRESS_FRAMES 0xF

/// Right shift from the 1/512-pixel position to pixels.
#define ACTION_PROMPT_SUBPIXEL_SHIFT 9

/// Clamp limits for the 1/512-pixel position. After the shift, X stays in
/// [-160, 159] and Y in [-110, 110], in pixels from the screen center.
#define ACTION_PROMPT_FIXED_X_MIN (-0x14000)
#define ACTION_PROMPT_FIXED_X_MAX 0x13E00
#define ACTION_PROMPT_FIXED_Y_MIN (-0xDC00)
#define ACTION_PROMPT_FIXED_Y_MAX 0xDC00

/// One pad port's point-and-click action prompt.
///
/// Gameplay keeps two, one per port. Room and actor overlays share them.
/// `fixedX` and `fixedY` are the cursor in 1/512-pixel units. Each move clamps
/// them and stores the pixel position in `screen`. Seeding `screen` alone does
/// not move the cursor: the next move replaces it from the fixed-point
/// position. `cursorSpeed` scales stick and d-pad motion. `doublePressWindow`
/// is how many frames a second press may follow the first and still count when
/// the cursor has not moved. `mode` selects the cursor sprite. `buttons` holds
/// the confirm slot and the cancel slot.
typedef struct {
    s32                fixedX;               // Horizontal 1/512-pixel units from the screen center
    s32                fixedY;               // Vertical 1/512-pixel units from the screen center, increasing downward
    ActionPromptScreen screen;               // Pixel position derived from fixedX and fixedY
    s16                cursorSpeed;          // Motion multiplier (0 stopped, 0x80 aiming, 0x100 after reset)
    u16                doublePressWindow;    // Frames in which a second press at the same position is a double-press
    u8                 mode;                 // Cursor sprite (0 hidden, 1 idle, 2 over a hotspot)
    byte               pad[3];               // Aligns the button slots; nothing reads or writes these bytes
    union {
        RoomActionPromptButton slots[2];     // [0] confirm, [1] cancel
        u16                    halfwords[8]; // Same bytes; the cursor task walks four halfwords per slot
    } buttons;
} ActionPrompt;
STATIC_ASSERT_SIZEOF(ActionPrompt, 0x24);

#endif // GAMEPLAY_ACTION_PROMPT_H
