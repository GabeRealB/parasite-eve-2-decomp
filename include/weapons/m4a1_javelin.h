#ifndef INCLUDE_WEAPONS_M4A1_JAVELIN_H
#define INCLUDE_WEAPONS_M4A1_JAVELIN_H

#include "main/task_types.h"

/// Runs the Javelin's muzzle flare, six-segment guide beam and optional ground glow.
///
/// Expects the counted, initially zeroed `EffectWork` in `spawnArg2.pointer`
/// and the effect's coordinate body in `extra.coordBody`. The flare attaches
/// to the work's borrowed parent coordinate; the beam ends at the tracked
/// contact point, or 8000 coordinate units along the attachment's local +Y.
/// Ground glow joins only consecutive successful collision probes.
/// After 32 active ticks, or departure from the player's normal firing state,
/// four retirement ticks consume the colour steps and release the work/task.
/// Room-effect pause/hide freezes updates; cancellation releases the effect.
void m4a1JavelinGuideBeamTask(Task* task);

/// Plays the eight-frame Javelin contact flash at the effect's cached world position.
///
/// Expects the counted, initially zeroed `EffectWork` in `spawnArg2.pointer`
/// and a composed coordinate body in `extra.coordBody`. Uses texture frames
/// 0..7, a fixed size of 512 and a random angle in 4096 units per turn.
/// Pause/hide freezes animation; cancellation or frame 7 releases the work/task.
void m4a1JavelinContactFlashTask(Task* task);

/// Runs the M4A1 Javelin player attack, choosing rifle bursts or a tracked guide beam.
///
/// Primary input fires up to three rounds at four-tick intervals. Secondary
/// input starts the guide beam, delays contacts for six ticks and enables them
/// for 28 ticks; each dispatch updates or clears its tracked contact point.
/// Requires 88 free scratch bytes; only the leading GfxCoord is accessed here,
/// and the role of the trailing eight reserved bytes is unproven.
///
/// Requires live player `GameActor` work, its model and initialized native
/// animation slots, equipped weapon/contact storage and the matching weapon
/// overlay loaded throughout dispatch and owned effects. Phase 0 enters
/// normal mode state 4; later calls advance `GameActor::statePhase`. Frame counts
/// are dispatch ticks. Releases its scratch reservation before returning.
void m4a1JavelinAttackState(Task* playerTask);

#endif // INCLUDE_WEAPONS_M4A1_JAVELIN_H
