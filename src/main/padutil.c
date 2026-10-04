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

void Pad_PostEvent(s32 arg0, s32 arg1, s32 arg2, s32 arg3)
{
    PadState*            p;
    PadVibrationRequest* requests;
    PadVibrationRequest* request;
    s32                  i;
    s32                  durationPolls;
    u8                   idx;

    p = &gPadStates[arg0];
    if (gDisplayState.demoScene != DISPLAY_DEMO_NONE) {
        return;
    }

    i        = 0;
    requests = p->vibrationRequests[arg1];
    for (; i < ARRAY_SIZE(p->vibrationRequests[arg1]); i++) {
        idx     = p->nextVibrationSlot;
        request = &requests[idx];
        if (request->active == PAD_VIBRATION_INACTIVE) {
            break;
        }
        idx                  = idx + 1;
        p->nextVibrationSlot = idx;
        if (idx >= ARRAY_SIZE(p->vibrationRequests[arg1])) {
            p->nextVibrationSlot = 0;
        }
    }

    request->active         = PAD_VIBRATION_ACTIVE;
    durationPolls           = (s16)arg3 * PAD_VIBRATION_POLLS_PER_DURATION_UNIT;
    request->intensity      = arg2;
    request->pollsRemaining = durationPolls;

    idx                  = p->nextVibrationSlot + 1;
    p->nextVibrationSlot = idx;
    if (idx >= ARRAY_SIZE(p->vibrationRequests[arg1])) {
        p->nextVibrationSlot = 0;
    }
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

void Pad_ClearEvents(s32 arg0)
{
    PadState*            p;
    s32                  i;
    s32                  j;
    s32                  bankByteOffset;
    PadVibrationRequest* requests;

    p              = &gPadStates[arg0];
    i              = 0;
    bankByteOffset = OFFSET_OF(PadState, vibrationRequests);
    for (; i < ARRAY_SIZE(p->vibrationRequests); i++) {
        requests = p->vibrationRequests[i];
        for (j = 0; j < ARRAY_SIZE(p->vibrationRequests[i]); j++) {
            requests[j].active         = PAD_VIBRATION_INACTIVE;
            requests[j].intensity      = 0;
            requests[j].pollsRemaining = 0;
        }
        bankByteOffset += sizeof(p->vibrationRequests[i]);
    }
    p->nextVibrationSlot = 0;
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
