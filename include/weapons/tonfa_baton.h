#ifndef INCLUDE_WEAPONS_TONFA_BATON_H
#define INCLUDE_WEAPONS_TONFA_BATON_H

#include "main/task_types.h"

/// Records the tonfa baton's swing and draws its fading additive ribbon.
///
/// Requires the tonfa baton overlay loaded, a live single-coordinate task
/// body, and its owned, counted `EffectWork` in `spawnArg2.pointer`. The work's
/// borrowed parent is the weapon coordinate and must remain live. State 0
/// seeds both eight-frame histories without drawing; state 1 records and draws
/// seven ribbon segments. `spawnArg1.value` selects blue at zero, cyan otherwise.
/// The histories and selected tint belong to the overlay and are shared by
/// simultaneous instances. Translation uses game coordinate units.
/// Only running room effects advance age or draw; other control values below
/// cancellation retain the task. Age 31 or cancellation releases its work and
/// kills the task under `effectKillTask`'s lifetime rules. Keep the overlay and
/// parent loaded through dispatch and teardown. Requires a current view,
/// frame-arena room for seven quads and blend commands, and an initialized
/// scratch stack with 48 free bytes, released before return.
void tonfaBatonSwingTrailTask(Task* task);

/// Dispatches the attached tonfa baton model's lifecycle and pose update.
///
/// `task` must be live with a TMD body and a state in 0..3 (0 initialize,
/// 1 update pose, 2 defer removal, 3 teardown); spawning starts at state 0.
/// Pose updates require the player's live actor work and TMD body and the
/// baton's initialized root coordinate, parented to its attachment anchor.
/// The low nibble of `spawnArg1.value` selects 0 rest, 1 strike, otherwise
/// hold the angle; leaving the player's normal-mode attack resets it to rest.
/// Local angles use 4096 units per turn and the grip offset is 96 coordinate
/// units along the parent's Y axis. The tonfa baton overlay must remain loaded
/// through frame/exit dispatch; teardown follows `taskKill`'s lifetime rules.
void tonfaBatonModelTask(Task* task);

/// Runs the tonfa baton player strike, optional second strike and recovery.
///
/// After eight wind-up ticks, a new R1/R2 press queues the second strike.
/// Each strike enables pair contacts for its own window and reports one hit.
/// Active strike/recovery movement advances along the root's local forward
/// axis. Requires the live baton model and 24 free scratch bytes.
///
/// Requires live player `GameActor` work, its model and initialized native
/// animation slots, equipped weapon/contact storage and the matching weapon
/// overlay loaded throughout dispatch and owned effects. Phase 0 enters
/// normal mode state 4; later calls advance `GameActor::statePhase`. Frame counts
/// are dispatch ticks. Releases its scratch reservation before returning.
void tonfaBatonAttackState(Task* playerTask);

#endif // INCLUDE_WEAPONS_TONFA_BATON_H
