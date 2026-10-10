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

/// Borrowed context of this renderer's active wave task.
///
/// Set on the seed tick and left unchanged at teardown. Only one wave may use
/// this storage; the context must stay live through its final drawing tick.
extern ScreenWaveCtx* gScreenWaveContext;

/// Column sine oscillators for horizontal screen displacement.
///
/// The per-frame renderer uses entries 0..10; the prebuilt-grid renderer uses
/// 0..8. Any remaining declared capacity is unaccessed; its role is unproven.
extern ScreenWaveOscillator gScreenWaveColumnWaves[13];

/// Thirty active row sine oscillators for vertical screen displacement.
///
/// Seeded at wave startup and advanced on drawing updates. Any declared
/// entries beyond 29 are unaccessed; their role is unproven.
extern ScreenWaveOscillator gScreenWaveRowWaves[32];

/// Persistent ramp and tint context borrowed by the tunnel's scripted screen wave.
///
/// A non-positive event request seeds a one-frame rise to strength 96 with
/// RGB modulation 64/128/128. The task initializes frame/state; positive requests
/// store a low signed-halfword phase (1 falling, 2 finished). Only one wave may
/// borrow it at a time, and this overlay must stay loaded through its final tick.
extern ScreenWaveCtx gScreenWaveSpawnCtx;

#endif // SRC_ROOMS_NEO_ARK_SUBMARINE_TUNNEL_NEO_ARK_SUBMARINE_TUNNEL_PRIVATE_H
