#ifndef INCLUDE_PE_LIFEDRAIN_H
#define INCLUDE_PE_LIFEDRAIN_H

#include "main/task_types.h"

/// Advances the short-lived rising spark emitted by Life Drain.
///
/// Requires a counted coordinate-body effect task with cleared, owned
/// `EffectWork` in `spawnArg2.pointer` and a live coordinate parent chain.
/// `spawnArg1.value` bits 0..11 give the billboard sizing numerator; higher
/// bits are ignored. After initialization, the spark moves four parent-space
/// Y units per tick and draws on alternate ticks with a fixed random rotation.
/// From cleared work it releases the work and task at age 16; parent-task
/// teardown can end it earlier. It performs no room-control pause check.
void lifedrainRisingSparkTask(Task* task);

/// Expands and fades one of Life Drain's three tilted glow bands.
///
/// Requires a counted coordinate-body effect task with owned `EffectWork`
/// in `spawnArg2.pointer`, initial state zero and a live coordinate parent
/// chain. `spawnArg1.value` bits 0..11 select the local Z rotation in 4096
/// units per turn. The active attachment's level digit must be 1..3; its
/// tuning row is captured on initialization.
///
/// Each running tick grows the inner radius and width in game-coordinate
/// units, draws an additive band, then reduces blue brightness by eight;
/// red and green are half of blue. The initialization tick also draws.
/// Nonzero PE effect control below the cancellation threshold freezes the
/// task without drawing; cancellation or brightness below nine releases
/// both work and task. With uninterrupted level 1..3 tuning, it draws for
/// 15, 21 or 27 running ticks respectively. GPU packets live for the frame.
void lifedrainExpandingGlowBandTask(Task* task);

/// Collects drained health and sequences Life Drain's flash and blue funnel.
///
/// Requires a counted coordinate-body effect with cleared, owned `EffectWork`
/// in `spawnArg2.pointer`, a live borrowed parent and active PE level 1..3.
/// Publishes itself as the collector for hit motes, which join its teardown
/// tree and add their `spawnArg1.value` to its own. Seeds 8/12/16 wedge yaws
/// and spawns three child expanding glow bands at Z angles 0, 682 and 1364
/// (4096 units per turn). The package permits one live collector at a time.
///
/// Age 3 plays the level's empty/success cue. At age 30, a nonzero collected
/// count grants `gSceneCombatState.lifeDrainHp`, capped at maximum HP, then
/// grows and fades the funnel, shedding child sparks. With no count, skips
/// the funnel. Cancellation in either opening state also grants banked HP
/// only when the count is nonzero. A held attachment or PE cancellation
/// releases the task; its published handle is not cleared on teardown.
void lifedrainCastTask(Task* task);

/// Drifts from a Life Drain hit, then steers toward the player's part-one origin.
///
/// Requires a counted coordinate-body effect with cleared `EffectWork` in
/// `spawnArg2.pointer`, a live published `lifedrainCastTask`, and live player
/// coordinates. Initialization joins the collector's teardown tree and adds
/// `spawnArg1.value` to its collected count. Three random draws seed signed
/// parent-frame velocity; the mote moves on the initialization tick.
///
/// The PE level index is stored in `scale` but does not select sizing: the
/// unwritten, cleared `step` selects row zero, giving spark size 1024 and mote
/// size 768 at every level. Odd ages draw two billboards and may shed a child
/// spark. At age 15, the player displacement is transformed and scaled by
/// `(4608 / (30 - age)) / 4096` into `pos`, without normalization. Later ticks
/// move each velocity component by +/-16 toward that target and recompute it.
/// Age 30 moves once more before release. A held attachment or PE cancellation
/// releases it immediately; the collector and Life Drain overlay stay live.
void lifedrainMoteTask(Task* task);

#endif // INCLUDE_PE_LIFEDRAIN_H
