/* Part of the action prompt library; see action_prompt.h. */

#ifndef ACTION_PROMPT_CURSOR_MOTION_HELPERS_DEFINED
#define ACTION_PROMPT_CURSOR_MOTION_HELPERS_DEFINED

/// Constrains an action-prompt cursor to its allowed screen region.
///
/// `prompt` must be non-NULL and writable. Clamps its signed 1/512-pixel
/// coordinates to inclusive X [-160, 159] and Y [-110, 110] pixel limits,
/// measured from the screen center with Y increasing downward. The caller
/// publishes `screen` afterward; this helper changes only `fixedX` and `fixedY`.
static inline void _actionPromptClampCursor(ActionPrompt* prompt)
{
    if (prompt->fixedX < ACTION_PROMPT_FIXED_X_MIN) {
        prompt->fixedX = ACTION_PROMPT_FIXED_X_MIN;
    } else if (prompt->fixedX > ACTION_PROMPT_FIXED_X_MAX) {
        prompt->fixedX = ACTION_PROMPT_FIXED_X_MAX;
    }
    if (prompt->fixedY < ACTION_PROMPT_FIXED_Y_MIN) {
        prompt->fixedY = ACTION_PROMPT_FIXED_Y_MIN;
    } else if (prompt->fixedY > ACTION_PROMPT_FIXED_Y_MAX) {
        prompt->fixedY = ACTION_PROMPT_FIXED_Y_MAX;
    }
}
#endif

/// Moves, classifies button presses and draws the selected action-prompt cursors.
///
/// `task->spawnArg1.value` selects port 0 with 1, port 1 with 2, and both with
/// any other value. Borrows the initialized per-port prompts and pad samples;
/// it neither owns task work nor changes the task state. Movement uses signed
/// Q12 left-stick axes: mouse-format input is linear, analog input is squared
/// with its sign restored. D-pad motion is added before clamping the signed
/// 1/512-pixel position to X [-160, 159] and Y [-110, 110] pixels.
///
/// Confirm is Cross; cancel is Circle or Square. A second press within
/// `doublePressWindow` nominal 60-Hz ticks at the last latched pixel position
/// reports `ACTION_PROMPT_BUTTON_DOUBLE_PRESS`. Classification uses the
/// previously published `screen`, before this call publishes the new position.
/// The u16 arm counters advance by `frameTicks` and wrap without saturation.
/// Requires the packet arena, ordering table and cursor textures needed by
/// `ACTION_PROMPT_DRAW_CURSOR`; hidden mode still updates motion and buttons.
void ACTION_PROMPT_MOVE_CURSORS_TASK(Task* task)
{
    enum {
        ACTION_PROMPT_PORT_0_ONLY            = 1,
        ACTION_PROMPT_PORT_1_ONLY            = 2,
        ACTION_PROMPT_AXIS_SIGN_EXTEND_SHIFT = 16,
        ACTION_PROMPT_LINEAR_AXIS_SHIFT      = 5,
        ACTION_PROMPT_SQUARED_AXIS_SHIFT     = 21,
        ACTION_PROMPT_DPAD_BITS_SHIFT        = 12,
        ACTION_PROMPT_DPAD_EIGHTH_TURN       = 0x200, // Angles use 4096 units per turn
        ACTION_PROMPT_DPAD_NO_HEADING        = -1,
        ACTION_PROMPT_DPAD_PRODUCT_SHIFT     = 9,
        ACTION_PROMPT_BUTTON_HALFWORDS       = sizeof(ActionPromptButton) / sizeof(u16)
    };
    ActionPrompt* prompt;
    PadState*     pad;
    s32           port;
    s32           firstPort;
    s32           endPort;
    s32           inputFormat;
    s32           stickValue;
    s32           motionValue; // Stick step, then D-pad heading; one reused signed temporary
    s32           buttonMask;
    s32           cursorSpeed;
    s32           buttonSlot;
    s32           buttonHalfwordOffset;
    u16*          buttonState;
    u16*          framesSinceArm;

    switch (task->spawnArg1.value) {
        case ACTION_PROMPT_PORT_0_ONLY:
            firstPort = 0;
            endPort   = 1;
            break;
        case ACTION_PROMPT_PORT_1_ONLY:
            firstPort = 1;
            endPort   = PAD_PORT_COUNT;
            break;
        default:
            firstPort = 0;
            endPort   = PAD_PORT_COUNT;
            break;
    }

    for (port = firstPort; port < endPort; port++) {
        prompt      = &D_80114D28[port];
        pad         = &gPadStates[port];
        inputFormat = pad->inputFormat;
        // Integrate the stick response in subpixels, preserving signed rounding.
        if (inputFormat == PAD_INPUT_FORMAT_MOUSE) {
            cursorSpeed = prompt->cursorSpeed;
            motionValue = ((u16)pad->stickAxes[PAD_STICK_LEFT_X] << ACTION_PROMPT_AXIS_SIGN_EXTEND_SHIFT) >>
                          (ACTION_PROMPT_AXIS_SIGN_EXTEND_SHIFT + ACTION_PROMPT_LINEAR_AXIS_SHIFT);
            prompt->fixedX += motionValue * cursorSpeed * gDisplayState.frameTicks;
            motionValue     = ((u16)pad->stickAxes[PAD_STICK_LEFT_Y] << ACTION_PROMPT_AXIS_SIGN_EXTEND_SHIFT) >>
                          (ACTION_PROMPT_AXIS_SIGN_EXTEND_SHIFT + ACTION_PROMPT_LINEAR_AXIS_SHIFT);
            prompt->fixedY += motionValue * cursorSpeed * gDisplayState.frameTicks;
        } else if (inputFormat == PAD_INPUT_FORMAT_ANALOG) {
            stickValue  = pad->stickAxes[PAD_STICK_LEFT_X];
            motionValue = (stickValue * stickValue) >> ACTION_PROMPT_SQUARED_AXIS_SHIFT;
            if (stickValue < 0) {
                motionValue = -motionValue;
            }
            prompt->fixedX += motionValue * prompt->cursorSpeed * gDisplayState.frameTicks;
            stickValue      = pad->stickAxes[PAD_STICK_LEFT_Y];
            motionValue     = (stickValue * stickValue) >> ACTION_PROMPT_SQUARED_AXIS_SHIFT;
            if (stickValue < 0) {
                motionValue = -motionValue;
            }
            prompt->fixedY += motionValue * prompt->cursorSpeed * gDisplayState.frameTicks;
        }

        // Only the eight cardinal/diagonal D-pad combinations have a heading.
        switch (pad->buttons >> ACTION_PROMPT_DPAD_BITS_SHIFT) {
            case PAD_BUTTON_UP >> ACTION_PROMPT_DPAD_BITS_SHIFT:
                motionValue = 0;
                break;
            case (PAD_BUTTON_UP | PAD_BUTTON_RIGHT) >> ACTION_PROMPT_DPAD_BITS_SHIFT:
                motionValue = ACTION_PROMPT_DPAD_EIGHTH_TURN;
                break;
            case PAD_BUTTON_RIGHT >> ACTION_PROMPT_DPAD_BITS_SHIFT:
                motionValue = 2 * ACTION_PROMPT_DPAD_EIGHTH_TURN;
                break;
            case (PAD_BUTTON_RIGHT | PAD_BUTTON_DOWN) >> ACTION_PROMPT_DPAD_BITS_SHIFT:
                motionValue = 3 * ACTION_PROMPT_DPAD_EIGHTH_TURN;
                break;
            case PAD_BUTTON_DOWN >> ACTION_PROMPT_DPAD_BITS_SHIFT:
                motionValue = 4 * ACTION_PROMPT_DPAD_EIGHTH_TURN;
                break;
            case (PAD_BUTTON_DOWN | PAD_BUTTON_LEFT) >> ACTION_PROMPT_DPAD_BITS_SHIFT:
                motionValue = 5 * ACTION_PROMPT_DPAD_EIGHTH_TURN;
                break;
            case PAD_BUTTON_LEFT >> ACTION_PROMPT_DPAD_BITS_SHIFT:
                motionValue = 6 * ACTION_PROMPT_DPAD_EIGHTH_TURN;
                break;
            case (PAD_BUTTON_LEFT | PAD_BUTTON_UP) >> ACTION_PROMPT_DPAD_BITS_SHIFT:
                motionValue = 7 * ACTION_PROMPT_DPAD_EIGHTH_TURN;
                break;
            default:
                motionValue = ACTION_PROMPT_DPAD_NO_HEADING;
                break;
        }

        if (motionValue != ACTION_PROMPT_DPAD_NO_HEADING) {
            prompt->fixedY += (-rcos(motionValue) * prompt->cursorSpeed * gDisplayState.frameTicks) >> ACTION_PROMPT_DPAD_PRODUCT_SHIFT;
            prompt->fixedX += (rsin(motionValue) * prompt->cursorSpeed * gDisplayState.frameTicks) >> ACTION_PROMPT_DPAD_PRODUCT_SHIFT;
        }

        _actionPromptClampCursor(prompt);

        buttonState          = &prompt->buttons.halfwords[0];
        framesSinceArm       = &prompt->buttons.halfwords[1];
        buttonHalfwordOffset = 0;
        // Compare presses against the previously published pixel position.
        // Keep the independent state walk and indexed arm counter: a slot's
        // latched position is recovered from its framesSinceArm member.
        for (buttonSlot = 0; buttonSlot < ARRAY_SIZE(prompt->buttons.slots);
             buttonSlot++, buttonState += ACTION_PROMPT_BUTTON_HALFWORDS,
            buttonHalfwordOffset += ACTION_PROMPT_BUTTON_HALFWORDS) {
            buttonMask = (buttonSlot == 0) ? PAD_BUTTON_CROSS : PAD_BUTTON_CIRCLE | PAD_BUTTON_SQUARE;
            if (padCheckButtons(port, PAD_BUTTON_QUERY_PRESSED, buttonMask) != 0) {
                if (framesSinceArm[buttonHalfwordOffset] < prompt->doublePressWindow &&
                    PARENT_OF(framesSinceArm + buttonHalfwordOffset, ActionPromptButton, framesSinceArm)->lastPos.packed ==
                        prompt->screen.packed) {
                    *buttonState                         = ACTION_PROMPT_BUTTON_DOUBLE_PRESS;
                    framesSinceArm[buttonHalfwordOffset] = prompt->doublePressWindow;
                } else {
                    framesSinceArm[buttonHalfwordOffset] = 0;
                    PARENT_OF(framesSinceArm + buttonHalfwordOffset, ActionPromptButton, framesSinceArm)->lastPos.packed =
                        prompt->screen.packed;
                    *buttonState = ACTION_PROMPT_BUTTON_PRESSED;
                }
            } else if (padCheckButtons(port, PAD_BUTTON_QUERY_RELEASED, buttonMask) != 0) {
                *buttonState = ACTION_PROMPT_BUTTON_RELEASED;
            } else if (padCheckButtons(port, PAD_BUTTON_QUERY_HELD_ANY, buttonMask) != 0) {
                *buttonState = ACTION_PROMPT_BUTTON_HELD;
            } else {
                *buttonState = ACTION_PROMPT_BUTTON_NONE;
            }
            framesSinceArm[buttonHalfwordOffset] += gDisplayState.frameTicks;
        }

        // Publish and draw only after classifying presses at the old position.
        prompt->screen.xy.x = prompt->fixedX >> ACTION_PROMPT_SUBPIXEL_SHIFT;
        prompt->screen.xy.y = prompt->fixedY >> ACTION_PROMPT_SUBPIXEL_SHIFT;
        ACTION_PROMPT_DRAW_CURSOR(prompt->screen.xy.x, prompt->screen.xy.y, prompt->mode);
    }
}
