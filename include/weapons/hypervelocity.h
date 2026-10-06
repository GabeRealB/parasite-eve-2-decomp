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

void func_hypervelocity_8011D1E8(Task* task);

void func_hypervelocity_8011F6C0(Task* arg0);

#endif // INCLUDE_WEAPONS_HYPERVELOCITY_H
