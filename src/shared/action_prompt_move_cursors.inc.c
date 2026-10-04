/* Part of the action prompt library; see action_prompt.h. */

/// State 1 of the room's prompt script task: moves the action-prompt cursors
/// from the pads and draws them.
///
/// `Task::spawnArg1` picks the ports: 1 drives port 0 only, 2 port 1 only,
/// anything else both. Each port's analog stick (input format 0x12 linear, 0x73
/// squared) and then its d-pad, whose four bits pick one of eight headings,
/// move the prompt's 1/512-pixel position, which is clamped to the screen. The
/// confirm (0x40) and cancel (0xA0) buttons are classified into the prompt's
/// two button slots. A second press within `doublePressWindow` frames at an
/// unmoved cursor reports `ACTION_PROMPT_BUTTON_DOUBLE_PRESS` instead of
/// `ACTION_PROMPT_BUTTON_PRESSED`. `cursorSpeed` scales every step.
void actionPromptMoveCursors(Task* task)
{
    ActionPrompt* prompt;
    PadState*     pad;
    s32           port;
    s32           first;
    s32           count;
    s32           inputFormat;
    s32           stick;
    s32           step;
    s32           mask;
    s32           speed;
    s32           i;
    s32           idx;
    u16*          statep;
    u16*          heldp;

    switch (task->spawnArg1.value) {
        case 1:
            first = 0;
            count = 1;
            break;
        case 2:
            first = 1;
            count = 2;
            break;
        default:
            first = 0;
            count = 2;
            break;
    }

    for (port = first; port < count; port++) {
        prompt      = &D_80114D28[port];
        pad         = &gPadStates[port];
        inputFormat = pad->inputFormat;
        if (inputFormat == PAD_INPUT_FORMAT_MOUSE) {
            speed           = prompt->cursorSpeed;
            step            = ((u16)pad->stickAxes[PAD_STICK_LEFT_X] << 0x10) >> 0x15;
            prompt->fixedX += step * speed * gDisplayState.frameTicks;
            step            = ((u16)pad->stickAxes[PAD_STICK_LEFT_Y] << 0x10) >> 0x15;
            prompt->fixedY += step * speed * gDisplayState.frameTicks;
        } else if (inputFormat == PAD_INPUT_FORMAT_ANALOG) {
            stick = pad->stickAxes[PAD_STICK_LEFT_X];
            step  = (stick * stick) >> 0x15;
            if (stick < 0) {
                step = -step;
            }
            prompt->fixedX += step * prompt->cursorSpeed * gDisplayState.frameTicks;
            stick           = pad->stickAxes[PAD_STICK_LEFT_Y];
            step            = (stick * stick) >> 0x15;
            if (stick < 0) {
                step = -step;
            }
            prompt->fixedY += step * prompt->cursorSpeed * gDisplayState.frameTicks;
        }

        switch (pad->buttons >> 0xC) {
            case 1:
                step = 0x0;
                break;
            case 3:
                step = 0x200;
                break;
            case 2:
                step = 0x400;
                break;
            case 6:
                step = 0x600;
                break;
            case 4:
                step = 0x800;
                break;
            case 12:
                step = 0xA00;
                break;
            case 8:
                step = 0xC00;
                break;
            case 9:
                step = 0xE00;
                break;
            default:
                step = -1;
                break;
        }

        if (step != -1) {
            prompt->fixedY += (-rcos(step) * prompt->cursorSpeed * gDisplayState.frameTicks) >> 9;
            prompt->fixedX += (rsin(step) * prompt->cursorSpeed * gDisplayState.frameTicks) >> 9;
        }

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

        statep = &prompt->buttons.halfwords[0];
        heldp  = &prompt->buttons.halfwords[1];
        idx    = 0;
        // Four halfwords per slot. Indexing the frame counter makes the latched
        // position a displacement off that register.
        for (i = 0; i < 2; i++, statep += 4, idx += 4) {
            mask = (i == 0) ? PAD_BUTTON_CROSS : PAD_BUTTON_CIRCLE | PAD_BUTTON_SQUARE;
            if (padCheckButtons(port, PAD_BUTTON_QUERY_PRESSED, mask) != 0) {
                if (heldp[idx] < prompt->doublePressWindow &&
                    PARENT_OF(heldp + idx, ActionPromptButton, framesSinceArm)->lastPos.packed ==
                        prompt->screen.packed) {
                    *statep    = ACTION_PROMPT_BUTTON_DOUBLE_PRESS;
                    heldp[idx] = prompt->doublePressWindow;
                } else {
                    heldp[idx] = 0;
                    PARENT_OF(heldp + idx, ActionPromptButton, framesSinceArm)->lastPos.packed =
                        prompt->screen.packed;
                    *statep = ACTION_PROMPT_BUTTON_PRESSED;
                }
            } else if (padCheckButtons(port, PAD_BUTTON_QUERY_RELEASED, mask) != 0) {
                *statep = ACTION_PROMPT_BUTTON_RELEASED;
            } else if (padCheckButtons(port, PAD_BUTTON_QUERY_HELD_ANY, mask) != 0) {
                *statep = ACTION_PROMPT_BUTTON_HELD;
            } else {
                *statep = ACTION_PROMPT_BUTTON_NONE;
            }
            heldp[idx] += gDisplayState.frameTicks;
        }

        prompt->screen.xy.x = prompt->fixedX >> ACTION_PROMPT_SUBPIXEL_SHIFT;
        prompt->screen.xy.y = prompt->fixedY >> ACTION_PROMPT_SUBPIXEL_SHIFT;
        actionPromptDrawCursor(prompt->screen.xy.x, prompt->screen.xy.y, prompt->mode);
    }
}
