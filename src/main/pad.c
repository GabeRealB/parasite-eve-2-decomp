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
    volatile PadState*   base;
    s32                  offset;
    volatile PadState*   p;
    u8*                  ptr;
    u32                  i;
    PadRawPort*          pad;
    volatile PadRawPort* vpad;
    u32                  j;
    u8                   ff;
    s32                  stateAddress;

    half         = PAD_INPUT_FORMAT_UNAVAILABLE;
    one          = 1;
    base         = Pad_States;
    stateAddress = (s32)base;
    p            = base;
    offset       = 0;
    do {
        // Clear each complete state through its byte representation, then publish setup fields.
        ptr = (u8*)(offset + stateAddress);
        for (i = 0; i < sizeof(*p); i++) {
            *ptr++ = 0;
        }
        p->actuatorCommand[0] = 0;
        p->actuatorCommand[1] = 0;
        p->inputFormat        = half;
        p->modeSetupPending   = one;
        p++;
        offset += sizeof(*p);
    } while (p < base + ARRAY_SIZE(Pad_States));

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
