#include "main/pad.h"

#include "types.h"

#include "main/display.h"
#include "main/display_types.h"
#include "gameflow.h"
#include "pad.h"
#include "main/pad_types.h"
#include "pad_types.h"

enum { PAD_VIBRATION_POLLS_PER_DURATION_UNIT = 2 };

enum {
    PAD_INPUT_BLOCK_UPDATES = 61,
    PAD_SOFT_RESET_COMBO    = 0x90F,
};

/// Advances the controller's shared vibration-request cursor past `slot`.
///
/// `pad` must point to writable controller state and `slot` must be in 0..7.
/// Both eight-entry motor banks use this cursor; advancing past slot 7 wraps to 0.
static inline void _padAdvanceVibrationSlot(PadState* pad, u8 slot)
{
    slot                   = slot + 1;
    pad->nextVibrationSlot = slot;
    if (slot >= ARRAY_SIZE(pad->vibrationRequests[PAD_VIBRATION_MOTOR_BINARY])) {
        pad->nextVibrationSlot = 0;
    }
}

s32 padCheckButtons(s32 port, s32 mode, s32 mask)
{
    const PadState* pad;
    u16             buttons;

    pad = &gPadStates[port];
    switch (mode) {
        case PAD_BUTTON_QUERY_PRESSED:
            buttons = pad->pressedButtons;
            break;
        case PAD_BUTTON_QUERY_RELEASED:
            buttons = pad->releasedButtons;
            break;
        default:
            buttons = pad->buttons;
            break;
    }
    if (mode == PAD_BUTTON_QUERY_HELD_ALL) {
        return (buttons & mask) == mask;
    }
    return (buttons & mask) != 0;
}

void padPostVibrationRequest(s32 port, s32 motorBank, s32 intensity, s32 durationUnits)
{
    PadState*            pad;
    PadVibrationRequest* requests;
    PadVibrationRequest* request;
    s32                  slotsExamined;
    s32                  durationPolls;
    u8                   slot;

    pad = &gPadStates[port];
    if (gDisplayState.demoScene != DISPLAY_DEMO_NONE) {
        return;
    }

    // A full bank retains the last examined slot for replacement.
    slotsExamined = 0;
    requests      = pad->vibrationRequests[motorBank];
    for (; slotsExamined < ARRAY_SIZE(pad->vibrationRequests[motorBank]); slotsExamined++) {
        slot    = pad->nextVibrationSlot;
        request = &requests[slot];
        if (request->active == PAD_VIBRATION_INACTIVE) {
            break;
        }
        _padAdvanceVibrationSlot(pad, slot);
    }

    // Preserve the byte intensity and doubled halfword countdown conversions.
    request->active         = PAD_VIBRATION_ACTIVE;
    durationPolls           = (s16)durationUnits * PAD_VIBRATION_POLLS_PER_DURATION_UNIT;
    request->intensity      = intensity;
    request->pollsRemaining = durationPolls;

    _padAdvanceVibrationSlot(pad, pad->nextVibrationSlot);
}

void Pad_SetCooldown(s32 arg0)
{
    volatile PadState* p;

    p                  = &gPadStates[arg0];
    p->inputBlockPolls = PAD_INPUT_BLOCK_UPDATES;
}

void padClearInputBlock(s32 port)
{
    PadState* pad;

    pad                  = &gPadStates[port];
    pad->inputBlockPolls = 0;
}

s32 Pad_ReadButtonsInv(s32 arg0)
{
    u16         sp;
    PadRawPort* base;

    base          = Pad_RawPorts;
    ((u8*)&sp)[1] = base[arg0].buttonsHigh;
    ((u8*)&sp)[0] = base[arg0].buttonsLow;
    return (u16)~sp;
}

void padClearVibrationRequests(s32 port)
{
    PadState*            pad;
    s32                  motorBank;
    s32                  slot;
    PadVibrationRequest* requests;

    pad       = &gPadStates[port];
    motorBank = 0;
    for (; motorBank < ARRAY_SIZE(pad->vibrationRequests); motorBank++) {
        requests = pad->vibrationRequests[motorBank];
        for (slot = 0; slot < ARRAY_SIZE(pad->vibrationRequests[motorBank]); slot++) {
            requests[slot].active         = PAD_VIBRATION_INACTIVE;
            requests[slot].intensity      = 0;
            requests[slot].pollsRemaining = 0;
        }
    }
    pad->nextVibrationSlot = 0;
}

s32 Pad_CheckSpecialCombo(void)
{
    volatile PadState* p;
    u16                val;
    s32                result;

    p   = gPadStates;
    val = p->buttons;
    if (val == PAD_SOFT_RESET_COMBO) {
        result = D_8005ED8A == PAD_SOFT_RESET_COMBO;
    } else {
        result = 0;
    }
    D_8005ED8A = val;
    if (p->inputBlockPolls != 0) {
        D_8005ED8A = 0;
        result     = 0;
    }
    return result;
}
