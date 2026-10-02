#ifndef GAMEPLAY_ACTION_PROMPT_H
#define GAMEPLAY_ACTION_PROMPT_H

#include "common.h"

/// Signed screen position of an action-prompt cursor, in pixels.
///
/// The origin is the screen center and Y increases downward, the same space as
/// an `ActionPromptHotspot`: a full-screen hotspot covers [-160, -120, 320, 240].
/// Each frame the cursor task stores the pixel coordinate of its 1/512-pixel
/// position here; those writes stay within X [-160, 159] and Y [-110, 110].
/// The two shorts occupy one word, so a button slot can latch the position and
/// tell whether the cursor has moved.
typedef struct {
    s16 x; // Horizontal pixels from the screen center
    s16 y; // Vertical pixels from the screen center, increasing downward
} ActionPromptCursorPos;
STATIC_ASSERT_SIZEOF(ActionPromptCursorPos, 4);

/// `id` of the entry that ends an action-prompt hotspot table. Not a choice.
#define ACTION_PROMPT_HOTSPOT_END (-1)

/// One entry of an action-prompt hotspot table.
///
/// Rooms and actors test the action cursor against these tables. The rectangle
/// uses the same center-origin pixel space as `ActionPromptCursorPos`; a
/// full-screen entry covers [-160, -120, 320, 240]. The shared hit test treats
/// the far edges `x + w` and `y + h` as inside. `id` is the value the owner
/// copies when the player confirms the entry. Several entries may share one
/// `id`, and any value other than `ACTION_PROMPT_HOTSPOT_END`, including
/// other negatives, is a choice the owner defines. `promptKind` is the display
/// mode the owner forwards when it opens the prompt for that choice. `hit` is
/// set on every entry whose rectangle contains the cursor and cleared on the
/// others, so more than one entry can be hit at once.
typedef struct {
    s16 x;          // Left edge, pixels from the screen center
    s16 y;          // Top edge, pixels from the screen center, increasing downward
    s16 w;          // Width in pixels
    s16 h;          // Height in pixels
    s16 id;         // Choice copied on confirm; ACTION_PROMPT_HOTSPOT_END ends the table
    u8  promptKind; // Display mode forwarded when the prompt opens for this choice
    s8  hit;        // 1 while the cursor is inside this rectangle, otherwise 0
} ActionPromptHotspot;
STATIC_ASSERT_SIZEOF(ActionPromptHotspot, 0xC);

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

/// Confirm or cancel slot of one action prompt.
///
/// `state` classifies the button this frame. `framesSinceArm` counts frames
/// since the slot last latched a press. A second press while that count is
/// still inside `doublePressWindow`, and while the cursor is still at
/// `lastPos`, is a double-press; the count is then set to the window so the
/// following press latches again. `lastPos` is that latched cursor position,
/// compared as one word. Slot 0 watches confirm (0x40) and slot 1 watches
/// cancel (0xA0).
typedef struct {
    u16                state;          // Press this frame (0 none, 1 held, 2 pressed, 3 released, 4 double-press)
    u16                framesSinceArm; // Frames since this slot last latched a press
    ActionPromptScreen lastPos;        // Cursor position latched with that press
} ActionPromptButton;
STATIC_ASSERT_SIZEOF(ActionPromptButton, 0x8);

/// Press classification stored in `ActionPromptButton::state`.
///
/// None means the button is up. Held means it is down and was already down.
/// Pressed is the frame it went down. Released is the frame it went up.
/// Double-press is a second press within `ActionPrompt::doublePressWindow`
/// frames of the latched one, at the same cursor position.
#define ACTION_PROMPT_BUTTON_NONE         0
#define ACTION_PROMPT_BUTTON_HELD         1
#define ACTION_PROMPT_BUTTON_PRESSED      2
#define ACTION_PROMPT_BUTTON_RELEASED     3
#define ACTION_PROMPT_BUTTON_DOUBLE_PRESS 4

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
    s32                fixedX;            // Horizontal 1/512-pixel units from the screen center
    s32                fixedY;            // Vertical 1/512-pixel units from the screen center, increasing downward
    ActionPromptScreen screen;            // Pixel position derived from fixedX and fixedY
    s16                cursorSpeed;       // Motion multiplier (0 stopped, 0x80 aiming, 0x100 after reset)
    u16                doublePressWindow; // Frames in which a second press at the same position is a double-press
    u8                 mode;              // Cursor sprite (0 hidden, 1 idle, 2 over a hotspot)
    byte               pad[3];            // Aligns the button slots; nothing reads or writes these bytes
    union {
        ActionPromptButton slots[2];      // [0] confirm, [1] cancel
        u16                halfwords[8];  // Same bytes; the cursor task walks four halfwords per slot
    } buttons;
} ActionPrompt;
STATIC_ASSERT_SIZEOF(ActionPrompt, 0x24);

#endif // GAMEPLAY_ACTION_PROMPT_H
