#ifndef INCLUDE_WEAPONS_M4A1_HAMMER_H
#define INCLUDE_WEAPONS_M4A1_HAMMER_H

#include "main/task_types.h"

/// Requests for the attachment glow, stored in `Task::spawnArg1.value`.
enum {
    M4A1_HAMMER_GLOW_OFF     = 0,
    M4A1_HAMMER_GLOW_IDLE    = 1,
    M4A1_HAMMER_GLOW_CHARGED = 2,
    M4A1_HAMMER_GLOW_RELEASE = 3
};

/// Updates the Hammer attachment's idle glow, charged spark burst and point light.
///
/// Bank-6 effect 0x29 owns an `EffectWork` in `spawnArg2.pointer` and a coordinate
/// body in `extra.coordBody`. The work's borrowed parent is the weapon coordinate;
/// it must remain live until teardown. `spawnArg1.value` selects `M4A1_HAMMER_GLOW_*`:
/// off hides the glow, idle flickers, charged runs for five active ticks before
/// returning to idle, and release frees the work and kills the task and its children.
/// Hidden room effects or a player model that skips active drawing suspend all
/// updates. Other room-effect pauses redraw idle/charged sprites without aging.
/// Publishes the attachment's cached composed position for `m4a1HammerImpactFlashTask`.
/// Sprite and beam endpoints use its signed low halfwords in `GsWSMATRIX` input space.
void m4a1HammerGlowTask(Task* task);

/// Draws the Hammer contact flash and its short beam back to the attachment.
///
/// Bank-6 effect 0x182 owns an `EffectWork` in `spawnArg2.pointer` and a coordinate
/// body in `extra.coordBody`. A nonzero `spawnArg1.value` attaches the flash to the
/// work's borrowed contact coordinate, which must remain live until teardown;
/// zero retains the spawn transform and repeats initialization on each tick.
/// Requires a live glow task already published by `m4a1HammerGlowTask`, whose
/// teardown also owns this task. Draws on odd age ticks, connects to the cached
/// attachment position only before age 8, and releases its work at age 25.
void m4a1HammerImpactFlashTask(Task* task);

/// Runs the M4A1 Hammer player attack, choosing rifle bursts or the attachment discharge.
///
/// Primary input fires up to three rounds at four-tick intervals. Secondary
/// input consumes an attachment load, widens and extends the weapon capsule
/// and requests the charged glow. Recovery installs a 12-tick cooldown.
/// Secondary discharge requires the live persistent Hammer glow task.
///
/// Requires live player `GameActor` work, its model and initialized native
/// animation slots, equipped weapon/contact storage and the matching weapon
/// overlay loaded throughout dispatch and owned effects. Phase 0 enters
/// normal mode state 4; later calls advance `GameActor::statePhase`. Frame counts
/// are dispatch ticks. Releases its scratch reservation before returning.
/// Reserves 80 scratch bytes for a temporary impact node, in addition to
/// the called helpers' reservations.
void m4a1HammerAttackState(Task* playerTask);

#endif // INCLUDE_WEAPONS_M4A1_HAMMER_H
