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

/// Zeroes `state` through its byte representation.
static inline void Pad_ClearState(PadState* state)
{
    u8* stateByte;
    u32 stateByteIndex;

    stateByte = (u8*)state;
    for (stateByteIndex = 0; stateByteIndex < sizeof(*state); stateByteIndex++) {
        *stateByte++ = 0;
    }
}

void Pad_Init(void)
{
    u32                  i;
    PadRawPort*          pad;
    volatile PadRawPort* vpad;
    u32                  j;
    u8                   ff;

    for (i = 0; i < ARRAY_SIZE(gPadStates); i++) {
        // Clear each complete state, then publish setup fields.
        Pad_ClearState(&gPadStates[i]);
        gPadStates[i].actuatorCommand[0] = 0;
        gPadStates[i].actuatorCommand[1] = 0;
        gPadStates[i].inputFormat        = PAD_INPUT_FORMAT_UNAVAILABLE;
        gPadStates[i].modeSetupPending   = 1;
    }

    pad = Pad_RawPorts;
    PadInitDirect((u8*)pad, (u8*)(pad + 1));
    j = 0;
    PadStartCom();
    vpad = pad;
    for (; j < 2; j++) {
        ff                = 0xFF;
        vpad->buttonsHigh = ff;
        vpad->buttonsLow  = ff;
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
