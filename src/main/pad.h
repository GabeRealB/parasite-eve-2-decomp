#ifndef MAIN_PRIVATE_PAD_H
#define MAIN_PRIVATE_PAD_H

#include "types.h"

#include "pad_types.h"

extern PadRawPort Pad_RawPorts[2];

void Pad_Init(void);

/// Polls controller port 0's setup, analog axes and timed vibration once per VSync.
///
/// Requires initialized libpad communication and scratch-stack storage. Requests
/// analog mode when supported and aligns the persistent two-byte actuator buffer.
/// Vibration countdowns advance only in serviced connection states, including
/// their expiry poll; the saved preference gates output after mixing. Legacy
/// controllers use the 0x40/binary encoding instead of the aligned motor pair.
///
/// Four wire axes normalize to signed Q12 (-4096..4096) outside a 24-unit raw
/// dead zone. Entering analog mode resets centers to 128 and clamps them so each
/// divisor is positive. Unusable modes supply all-released raw button bytes;
/// digital and unusable modes clear axes. Main-loop input updates derive button
/// edges separately. Port 1's stored state and receive buffer are untouched.
void padPollPort0(void);

/// Tests whether port 0 holds only the six-button soft-reset chord on consecutive calls.
///
/// Returns 1 for Select + Start + L1 + L2 + R1 + R2 on this and the preceding
/// call, otherwise 0. Uses the stored processed held mask without polling it.
/// Each call replaces the previous sample; an active input block clears that
/// history and forces 0. A continued hold keeps returning 1 until interrupted.
s32 padCheckSoftResetCombo(void);

void Pad_UpdatePort0(void);

#endif // MAIN_PRIVATE_PAD_H
