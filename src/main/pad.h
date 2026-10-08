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

/// Updates controller port 0's held buttons, edges and modal UI direction repeat.
///
/// Uses the latest receive-buffer sample without polling libpad. Unblocked
/// input adds left-stick D-pad bits beyond +/-2048 in Q12, then applies an
/// active diagnostic override only during game-loop presentation, clearing all
/// four axes first. The gameplay override entry must be loaded when selected.
/// Pressed and released masks compare with the previously stored held word.
///
/// While a UI is open, an unchanged D-pad mask accumulates nominal 60-Hz
/// `frameTicks`: at 30 ticks, held directions repeat as presses and the byte
/// counter resets to 22. Byte addition wraps before the threshold test; repeat
/// time is retained while UI input is closed or blocked.
///
/// Blocked updates clear all three masks and decrement the volatile byte
/// countdown once. The expiry update restores only raw held buttons, without
/// edges, stick directions, overrides or repeat. Requires an initialized,
/// halfword-aligned scratch stack with six free bytes; the block is released
/// before returning. Port 1 is untouched.
void padUpdatePort0(void);

#endif // MAIN_PRIVATE_PAD_H
