#ifndef INCLUDE_WEAPONS_HYPERVELOCITY_H
#define INCLUDE_WEAPONS_HYPERVELOCITY_H

#include "gameplay/animation.h"

#include "main/task_types.h"
#include "main/tmd_types.h"

/// Native player animation bank for the equipped Hypervelocity.
///
/// Gameplay selects this bank at index 23 (character-1 base 1 plus weapon 22).
/// The 47 base slots hold clip pointers or NULL; slots 0, 45 and 46 are NULL.
/// The following 32 extension words start cleared and remain writable for
/// scripted copies, which can include non-pointer payloads. Play only slots
/// known to contain clips. The bank and its borrowed sets remain valid only
/// while the Hypervelocity overlay stays loaded.
extern AnimationBank gHypervelocityPlayerAnimationBank;

/// Root body of the player's held Hypervelocity, supplied by bank-7 descriptor 0x55.
///
/// One part owns 50 vertices and 37 lighting normals. Its writable source and
/// command stream stay in the loaded weapon overlay while any model borrows them;
/// creation resolves draw handlers in place. Each primitive-buffer half holds
/// one FT4, two GT3s and 38 GT4s. The root parents the charging slide and hinge.
extern TmdSource gHypervelocityRootModel;

/// Sliding Hypervelocity component, supplied by bank-7 descriptor 0x70.
///
/// One part owns sixteen vertices and sixteen lighting normals; each primitive
/// buffer half holds fourteen GT4s. Its task translates along the root's local
/// Y axis during charging. The writable source and stream remain loaded while
/// its model borrows them; creation resolves draw handlers in place.
extern TmdSource gHypervelocitySlideModel;

/// Hinged Hypervelocity component, supplied by bank-7 descriptor 0x74.
///
/// One part owns twelve vertices and sixteen lighting normals; each primitive
/// buffer half holds one FT4 and ten GT4s. Its task pitches about local X as the
/// second charge stage opens it. The writable source and stream remain loaded
/// while its model borrows them; creation resolves draw handlers in place.
extern TmdSource gHypervelocityHingeModel;

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

/// Flies a Hypervelocity round, lights its path and collapses its visuals on a grid hit.
///
/// Bank-6 effect 0x00C owns a coordinate body, counted `EffectWork` in
/// `spawnArg2.pointer` and a separately allocated collision block in `work`.
/// Requires the loaded weapon/gameplay overlays, live player model, current
/// view/GTE state, frame-arena room and initialized scratch storage.
/// Initialization copies only the player's 3x3 coefficients and rotates the
/// 1024-unit local muzzle velocity into `move`. `scale` is visual brightness;
/// `angle` is the visual radius/perspective-size numerator in game units;
/// `period` is a fixed sprite rotation in 4096 units per turn. Refreshes
/// transient point-light slot zero. Flight ends at active age 21 or a grid
/// segment hit; pair contacts are cleared without stopping flight. A grid hit
/// unlinks the body and shrinks the radius by 64 per tick until below 128.
/// Non-running effect control decrements age; cancellation unlinks initialized
/// collision and releases the effect. Exit frees the separate collision block.
void hypervelocityRoundTask(Task* task);

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

/// Dispatches the Hypervelocity weapon root or one of its moving model components.
///
/// Bank-7 descriptors 0x56, 0x70 and 0x74 supply the root, slide and hinge models.
/// `Task::state` must be 0..3 (initialize, pose, request teardown, release).
/// The low nibble of `spawnArg1.value` is 0 root, 1 slide or 2 hinge; only the root
/// creates child models. Children borrow the root coordinate and lighting and
/// read its charge-motion flags. Requires live player/root models and the
/// matching weapon/gameplay overlays throughout dispatch. Release kills the
/// task and its children and schedules or performs model teardown.
void hypervelocityWeaponModelTask(Task* task);

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
