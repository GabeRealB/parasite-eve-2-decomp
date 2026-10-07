#ifndef MAIN_PRIVATE_PAD_H
#define MAIN_PRIVATE_PAD_H

#include "types.h"

#include "pad_types.h"

extern PadRawPort Pad_RawPorts[2];

void Pad_Init(void);

/// Tests whether port 0 holds only the six-button soft-reset chord on consecutive calls.
///
/// Returns 1 for Select + Start + L1 + L2 + R1 + R2 on this and the preceding
/// call, otherwise 0. Uses the stored processed held mask without polling it.
/// Each call replaces the previous sample; an active input block clears that
/// history and forces 0. A continued hold keeps returning 1 until interrupted.
s32 padCheckSoftResetCombo(void);

void Pad_UpdatePort0(void);

#endif // MAIN_PRIVATE_PAD_H
