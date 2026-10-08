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

/// Clears the complete input, setup and vibration state for one controller port.
///
/// `state` is a writable PadState borrowed for initialization, before polling
/// or controller communication starts. All sizeof(*state) bytes become zero;
/// the caller then installs the unavailable-input and pending-setup values.
static inline void _padClearState(PadState* state)
{
    u8* stateByte;
    u32 stateByteIndex;

    stateByte = (u8*)state;
    for (stateByteIndex = 0; stateByteIndex < sizeof(*state); stateByteIndex++) {
        *stateByte++ = 0;
    }
}

void padInit(void)
{
    enum { PAD_RAW_BUTTONS_RELEASED = 0xFF };
    u32                  portIndex;
    PadRawPort*          rawPorts;
    volatile PadRawPort* rawPort;
    u32                  rawPortIndex;

    for (portIndex = 0; portIndex < ARRAY_SIZE(gPadStates); portIndex++) {
        // Clear each complete state, then publish setup fields.
        _padClearState(&gPadStates[portIndex]);
        gPadStates[portIndex].actuatorCommand[PAD_VIBRATION_MOTOR_BINARY]   = 0;
        gPadStates[portIndex].actuatorCommand[PAD_VIBRATION_MOTOR_VARIABLE] = 0;
        gPadStates[portIndex].inputFormat                                   = PAD_INPUT_FORMAT_UNAVAILABLE;
        gPadStates[portIndex].modeSetupPending                              = true;
    }

    rawPorts = Pad_RawPorts;
    PadInitDirect((u8*)rawPorts, (u8*)(rawPorts + 1));
    rawPortIndex = 0;
    PadStartCom();
    // Failed exchanges leave response bytes stale, so seed all buttons as released.
    rawPort = rawPorts;
    for (; rawPortIndex < ARRAY_SIZE(Pad_RawPorts); rawPortIndex++) {
        rawPort->buttonsHigh = PAD_RAW_BUTTONS_RELEASED;
        rawPort->buttonsLow  = PAD_RAW_BUTTONS_RELEASED;
        rawPort++;
    }
}

void actorRenderResetLists(void)
{
    D_80071210                     = 0;
    gTmdList.next                  = NULL;
    gTmdList.prev                  = &gTmdList;
    gModelObjectCoordBodyList.next = NULL;
    gModelObjectCoordBodyList.prev = &gModelObjectCoordBodyList;
}
