/* Part of the action prompt library; see action_prompt.h. */

/// State 1 of the room's prompt script task: moves the action-prompt cursors
/// from the pads and draws them.
///
/// `Task::spawnArg1` picks the ports: 1 drives port 0 only, 2 port 1 only,
/// anything else both. Each port's analog stick (input format 0x12 linear, 0x73
/// squared) and then its d-pad, whose four bits pick one of eight headings,
/// move the prompt's 1/512-pixel position, which is clamped to the screen. The
/// confirm (0x40) and cancel (0xA0) buttons are classified into the prompt's
/// two button slots, a second press within `field_E` frames at an unmoved
/// cursor reporting state 4 instead of 2. `targetId` is the cursor speed here.
void actionPromptMoveCursors(Task* task)
{
    RoomActionPrompt* prompt;
    PadState*         pad;
    s32               port;
    s32               first;
    s32               count;
    s32               inputFormat;
    s32               stick;
    s32               step;
    s32               mask;
    s32               speed;
    s32               i;
    s32               idx;
    u16*              statep;
    u16*              heldp;

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
        pad         = &Pad_States[port];
        inputFormat = pad->inputFormat;
        if (inputFormat == PAD_INPUT_FORMAT_MOUSE) {
            speed            = prompt->targetId;
            step             = ((u16)pad->stickAxes[PAD_STICK_LEFT_X] << 0x10) >> 0x15;
            prompt->field_0 += step * speed * gDisplayState.frameTicks;
            step             = ((u16)pad->stickAxes[PAD_STICK_LEFT_Y] << 0x10) >> 0x15;
            prompt->field_4 += step * speed * gDisplayState.frameTicks;
        } else if (inputFormat == PAD_INPUT_FORMAT_ANALOG) {
            stick = pad->stickAxes[PAD_STICK_LEFT_X];
            step  = (stick * stick) >> 0x15;
            if (stick < 0) {
                step = -step;
            }
            prompt->field_0 += step * prompt->targetId * gDisplayState.frameTicks;
            stick            = pad->stickAxes[PAD_STICK_LEFT_Y];
            step             = (stick * stick) >> 0x15;
            if (stick < 0) {
                step = -step;
            }
            prompt->field_4 += step * prompt->targetId * gDisplayState.frameTicks;
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
            prompt->field_4 += (-rcos(step) * prompt->targetId * gDisplayState.frameTicks) >> 9;
            prompt->field_0 += (rsin(step) * prompt->targetId * gDisplayState.frameTicks) >> 9;
        }

        if (prompt->field_0 < -0x14000) {
            prompt->field_0 = -0x14000;
        } else if (prompt->field_0 > 0x13E00) {
            prompt->field_0 = 0x13E00;
        }
        if (prompt->field_4 < -0xDC00) {
            prompt->field_4 = -0xDC00;
        } else if (prompt->field_4 > 0xDC00) {
            prompt->field_4 = 0xDC00;
        }

        statep = &prompt->buttons.halfwords[0];
        heldp  = &prompt->buttons.halfwords[1];
        idx    = 0;
        for (i = 0; i < 2; i++, statep += 4, idx += 4) {
            mask = (i == 0) ? 0x40 : 0xA0;
            if (Pad_CheckButtons(port, 1, mask) != 0) {
                if (heldp[idx] < prompt->field_E &&
                    PARENT_OF(heldp + idx, RoomActionPromptButton, heldFrames)->lastPos == prompt->screen.packed) {
                    *statep    = 4;
                    heldp[idx] = prompt->field_E;
                } else {
                    heldp[idx]                                                          = 0;
                    PARENT_OF(heldp + idx, RoomActionPromptButton, heldFrames)->lastPos = prompt->screen.packed;
                    *statep                                                             = 2;
                }
            } else if (Pad_CheckButtons(port, 3, mask) != 0) {
                *statep = 3;
            } else if (Pad_CheckButtons(port, 0, mask) != 0) {
                *statep = 1;
            } else {
                *statep = 0;
            }
            heldp[idx] += gDisplayState.frameTicks;
        }

        prompt->screen.xy.x = prompt->field_0 >> 9;
        prompt->screen.xy.y = prompt->field_4 >> 9;
        actionPromptDrawCursor(prompt->screen.xy.x, prompt->screen.xy.y, prompt->mode);
    }
}
