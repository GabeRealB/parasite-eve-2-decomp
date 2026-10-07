#ifndef INCLUDE_WEAPONS_HYPERVELOCITY_H
#define INCLUDE_WEAPONS_HYPERVELOCITY_H

#include "main/task_types.h"

/// Expands and fades one of the hypervelocity round's discharge cones.
///
/// Bank-6 effect 0x00B owns a coordinate body and a separate counted
/// `EffectWork` in `spawnArg2.pointer`; `work` must not alias that allocation.
/// State 0 seeds brightness 128 and collar half-extent 512 game units, then
/// state 1 draws both walls with grayscale tint. Each active tick adds 96 to
/// the half-extent and subtracts 8 from brightness, ending below 6 after 16
/// draws. `EffectWork::age` counts active ticks; `scale` is brightness and
/// `angle` is the half-extent. Nonzero room effect control pauses the task
/// below its cancellation threshold and releases it at or above that threshold.
/// The hypervelocity overlay must remain loaded; teardown invalidates the task
/// and its owned effect work and coordinate body.
void hypervelocityDischargeConeTask(Task* task);

void func_hypervelocity_8011D830(Task* task);

/// Expands and fades the blue-white shock ring at a hypervelocity round's launch.
///
/// Bank-6 effect 0x00D owns a coordinate body and a separate counted
/// `EffectWork` in `spawnArg2.pointer`; `work` must not alias that allocation.
/// State 0 seeds brightness 240 and inner radius 256 game units, then state 1
/// draws a raised band with red and green at half the blue brightness. Each
/// active tick adds 64 to the radius and subtracts 16 from brightness, ending
/// below 16 after 15 draws. `EffectWork::age` counts active ticks; `scale` is
/// brightness and `angle` is the inner radius. Nonzero room effect control
/// pauses the task below its cancellation threshold and releases it at or above it.
/// The hypervelocity overlay must remain loaded; teardown invalidates the task
/// and its owned effect work and coordinate body.
void hypervelocityShockRingTask(Task* task);

/// Runs the Hypervelocity's persistent charge glow, discharge and cooling smoke.
///
/// Bank-6 effect 0x24 owns a coordinate body and `EffectWork` in `spawnArg2.pointer`,
/// attached to the work's borrowed muzzle parent. spawnArg1.value is a mutable
/// request/countdown: 1 starts charging, negative cancels, and values at least
/// 2 start the timed discharge. After 65 charging ticks it supplies a 24-tick
/// discharge itself. The round is spawned when that countdown reaches zero.
/// Cooling returns to idle when effect age reaches 111 or on a negative request.
/// Non-running room effects pause dispatch; cancellation control resets to idle
/// without releasing this persistent effect. Refreshes transient light slot 1.
/// Requires the loaded weapon/gameplay overlays, a live player model with hand
/// coordinates 15 and 18, current view/GTE state and room for spawned effects.
/// Parent, task and owned work remain live until the player tears them down.
void hypervelocityChargeEffectTask(Task* task);

void func_hypervelocity_8011F6C0(Task* arg0);

/// Runs the Hypervelocity player charge, cancellation and recoil phases.
///
/// Held R1 or R2 charges for 90 ticks, opening the second model component at
/// 60. A completed charge consumes one round and starts 21 recoil ticks;
/// the final eighteen push backward along the root's local forward axis.
/// Requires the live persistent charge effect and weapon model tasks, plus
/// 24 free scratch bytes for the recoil vectors.
///
/// Requires live player `GameActor` work, its model and initialized native
/// animation slots, equipped weapon/contact storage and the matching weapon
/// overlay loaded throughout dispatch and owned effects. Phase 0 enters
/// normal mode state 4; later calls advance `GameActor::statePhase`. Frame counts
/// are dispatch ticks. Releases its scratch reservation before returning.
void hypervelocityAttackState(Task* playerTask);

#endif // INCLUDE_WEAPONS_HYPERVELOCITY_H
