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

void func_lifedrain_8012EF48(Task* arg0);

void func_lifedrain_8012FAF8(Task* arg0);

#endif // INCLUDE_PE_LIFEDRAIN_H
