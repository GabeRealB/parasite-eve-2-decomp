#include "main/pad.h"

#include "types.h"

#include "main/display.h"
#include "main/display_types.h"
#include "gameflow.h"
#include "pad.h"
#include "main/pad_types.h"
#include "pad_types.h"

enum { PAD_VIBRATION_POLLS_PER_DURATION_UNIT = 2 };

s32 Pad_CheckButtons(s32 arg0, s32 arg1, s32 arg2)
{
    PadState* p;
    u16       val;

    p = &Pad_States[arg0];
    switch (arg1) {
        case 1:
            val = p->prevButtons;
            break;
        case 3:
            val = p->triggered;
            break;
        default:
            val = p->buttons;
            break;
    }
    if (arg1 == 2) {
        return (val & arg2) == arg2;
    }
    return (val & arg2) != 0;
}

void Pad_PostEvent(s32 arg0, s32 arg1, s32 arg2, s32 arg3)
{
    PadState*            p;
    PadVibrationRequest* requests;
    PadVibrationRequest* request;
    s32                  i;
    s32                  durationPolls;
    u8                   idx;

    p = &Pad_States[arg0];
    if (gDisplayState.demoScene != DISPLAY_DEMO_NONE) {
        return;
    }

    i        = 0;
    requests = p->events[arg1];
    for (; i < ARRAY_SIZE(p->events[arg1]); i++) {
        idx     = p->eventIdx;
        request = &requests[idx];
        if (request->active == PAD_VIBRATION_INACTIVE) {
            break;
        }
        idx         = idx + 1;
        p->eventIdx = idx;
        if (idx >= ARRAY_SIZE(p->events[arg1])) {
            p->eventIdx = 0;
        }
    }

    request->active         = PAD_VIBRATION_ACTIVE;
    durationPolls           = (s16)arg3 * PAD_VIBRATION_POLLS_PER_DURATION_UNIT;
    request->intensity      = arg2;
    request->pollsRemaining = durationPolls;

    idx         = p->eventIdx + 1;
    p->eventIdx = idx;
    if (idx >= ARRAY_SIZE(p->events[arg1])) {
        p->eventIdx = 0;
    }
}

void Pad_SetCooldown(s32 arg0)
{
    volatile PadState* p;

    p           = &Pad_States[arg0];
    p->cooldown = 0x3D;
}

void Pad_ClearCooldown(s32 arg0)
{
    volatile PadState* p;

    p           = &Pad_States[arg0];
    p->cooldown = 0;
}

s32 Pad_ReadButtonsInv(s32 arg0)
{
    u16         sp;
    PadRawPort* base;

    base          = Pad_RawPorts;
    ((u8*)&sp)[1] = base[arg0].field_2;
    ((u8*)&sp)[0] = base[arg0].field_3;
    return (u16)~sp;
}

void Pad_ClearEvents(s32 arg0)
{
    PadState*            p;
    s32                  i;
    s32                  j;
    s32                  bankByteOffset;
    PadVibrationRequest* requests;

    p              = &Pad_States[arg0];
    i              = 0;
    bankByteOffset = OFFSET_OF(PadState, events);
    for (; i < ARRAY_SIZE(p->events); i++) {
        requests = p->events[i];
        for (j = 0; j < ARRAY_SIZE(p->events[i]); j++) {
            requests[j].active         = PAD_VIBRATION_INACTIVE;
            requests[j].intensity      = 0;
            requests[j].pollsRemaining = 0;
        }
        bankByteOffset += sizeof(p->events[i]);
    }
    p->eventIdx = 0;
}

s32 Pad_CheckSpecialCombo(void)
{
    volatile PadState* p;
    u16                val;
    s32                result;

    p   = Pad_States;
    val = p->buttons;
    if (val == 0x90F) {
        result = D_8005ED8A == 0x90F;
    } else {
        result = 0;
    }
    D_8005ED8A = val;
    if (p->cooldown != 0) {
        D_8005ED8A = 0;
        result     = 0;
    }
    return result;
}
