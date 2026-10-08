#ifndef GAMEPLAY_ENDING_H
#define GAMEPLAY_ENDING_H

#include "gameplay/pad_script.h"

#include "main/task_types.h"

/// Starts the post-credits replay-award and cleared-save screen sequence.
///
/// The replay_bonus package must be loaded and remain loaded until its task
/// and panels finish. Requires the live completed-run save, gameplay catalogue,
/// restored menu display configuration and initialized active task list.
/// Only one award sequence may use the package's shared totals at a time.
/// Spawns a bodyless controller with both payload words zero; its callback
/// later loads the award UI, presents bonuses and offers saving before restart.
/// Returns the new task, or NULL if allocation fails; it does not wait.
Task* endingSpawnReplayAwardScreen(void);

/// Gameplay-resident ending scripts consumed by the pad-script tasks.
extern PadScriptCmd D_80114A24[4];

extern PadScriptVibrationSegment D_80114A34[3];

#endif // GAMEPLAY_ENDING_H
