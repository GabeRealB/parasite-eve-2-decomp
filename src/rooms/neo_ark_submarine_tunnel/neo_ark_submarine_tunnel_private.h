#ifndef SRC_ROOMS_NEO_ARK_SUBMARINE_TUNNEL_NEO_ARK_SUBMARINE_TUNNEL_PRIVATE_H
#define SRC_ROOMS_NEO_ARK_SUBMARINE_TUNNEL_NEO_ARK_SUBMARINE_TUNNEL_PRIVATE_H

#include "types.h"

#include "overlay.h"

/// Entry scene/control latch for this overlay load (0 pending, 1 handled).
///
/// Claiming the variant-1 event, the first variant-3 entry scene or the fallback
/// arrival-control release sets it. Reloading the overlay clears the latch;
/// persistent story progress and the event-seen flag are separate.
extern u8 gNeoArkSubmarineTunnelEntryActionHandled;

extern ScreenWaveCtx* gScreenWaveCtx;

extern ScreenWaveOscillator gScreenWaveColumns[13];

extern ScreenWaveOscillator gScreenWaveRows[32];

/// Persistent ramp and tint context borrowed by the tunnel's scripted screen wave.
///
/// A non-positive event request seeds a one-frame rise to strength 96 with
/// RGB modulation 64/128/128. The task initializes frame/state; positive requests
/// store a low signed-halfword phase (1 falling, 2 finished). Only one wave may
/// borrow it at a time, and this overlay must stay loaded through its final tick.
extern ScreenWaveCtx gScreenWaveSpawnCtx;

#endif // SRC_ROOMS_NEO_ARK_SUBMARINE_TUNNEL_NEO_ARK_SUBMARINE_TUNNEL_PRIVATE_H
