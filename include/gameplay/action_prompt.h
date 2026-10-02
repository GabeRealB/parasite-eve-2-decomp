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

typedef union RoomActionPromptScreen {
    /* 0x0 */ ActionPromptCursorPos xy;
    /* 0x0 */ s32                   packed;
} RoomActionPromptScreen;

/// One of the two button slots at the tail of `RoomActionPrompt`. `state` is the
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

/// Gameplay-resident action-prompt state shared by room and actor overlays.
/// There are two of them, one per pad port.
///
/// A room's hotspot scan stores the id of the thing under the cursor in
/// `targetId` and a mode in `mode` (0 = nothing under the cursor, 1 = a hotspot
/// is highlighted, 2 = the hotspot is confirmed). `screen` holds the
/// coordinates handed to `func_800D4E78`, which parks them in the gameplay
/// globals the prompt's display task reads; `field_0` / `field_4` are the same
/// position in 1/512-pixel fixed point, which is what the analog stick and the
/// d-pad actually integrate into. `targetId` doubles as the cursor speed and
/// `field_E` as the double-press window. The three bytes at offset 0x11 have
/// no identified use; their original role is unresolved.
typedef struct RoomActionPrompt {
    /* 0x00 */ s32                    field_0;
    /* 0x04 */ s32                    field_4;
    /* 0x08 */ RoomActionPromptScreen screen;
    /* 0x0C */ s16                    targetId;
    /* 0x0E */ u16                    field_E;
    /* 0x10 */ u8                     mode;
    /* 0x11 */ byte                   pad_11[0x3];
    /// Whole button storage: the cursor loops walk halfwords across both slots.
    /// Recover a slot from its heldFrames address with PARENT_OF for lastPos.
    /* 0x14 */ union {
        RoomActionPromptButton slots[2];
        u16                    halfwords[8];
    } buttons;
} RoomActionPrompt;
STATIC_ASSERT_SIZEOF(RoomActionPrompt, 0x24);

#endif // GAMEPLAY_ACTION_PROMPT_H
