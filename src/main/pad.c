#include <psyq/sys/types.h>
#include <psyq/libpad.h>

#include "types.h"

#include "main/pad.h"
#include "main/pad_types.h"
#include "pad_types.h"
#include "main/tmd_types.h"

/* Define BSS before API headers to preserve first-declaration order. */
TmdListNode gTmdList;

TmdListNode gModelObjectCoordBodyList;

PadRawPort Pad_RawPorts[2];

s32 D_80071210;

#include "pad.h"

#include "main/tmd.h"

#include "gameplay/model_objects.h"

void Pad_Init(void)
{
    u16                  half;
    u8                   one;
    volatile PadState*   states;
    s32                  stateByteOffset;
    volatile PadState*   state;
    u8*                  stateByte;
    u32                  stateByteIndex;
    PadRawPort*          pad;
    volatile PadRawPort* vpad;
    u32                  j;
    u8                   ff;
    uintptr              statesAddress;

    half            = PAD_INPUT_FORMAT_UNAVAILABLE;
    one             = 1;
    states          = gPadStates;
    statesAddress   = (uintptr)states;
    state           = states;
    stateByteOffset = 0;
    do {
        // Clear each complete state through its byte representation, then publish setup fields.
        stateByte = (u8*)(stateByteOffset + statesAddress);
        for (stateByteIndex = 0; stateByteIndex < sizeof(*state); stateByteIndex++) {
            *stateByte++ = 0;
        }
        state->actuatorCommand[0] = 0;
        state->actuatorCommand[1] = 0;
        state->inputFormat        = half;
        state->modeSetupPending   = one;
        state++;
        stateByteOffset += sizeof(*state);
    } while (state < states + ARRAY_SIZE(gPadStates));

    pad = Pad_RawPorts;
    PadInitDirect((u8*)pad, (u8*)(pad + 1));
    j = 0;
    PadStartCom();
    vpad = pad;
    for (; j < 2; j++) {
        ff            = 0xFF;
        vpad->field_2 = ff;
        vpad->field_3 = ff;
        vpad++;
    }
}

void Tmd_InitLists(void)
{
    D_80071210                     = 0;
    gTmdList.next                  = NULL;
    gTmdList.prev                  = &gTmdList;
    gModelObjectCoordBodyList.next = NULL;
    gModelObjectCoordBodyList.prev = &gModelObjectCoordBodyList;
}
