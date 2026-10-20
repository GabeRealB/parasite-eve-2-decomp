#ifndef GAMEPLAY_ENDING_H
#define GAMEPLAY_ENDING_H

#include "main/task_types.h"

/// Starts the post-credits replay-award and cleared-save screen sequence.
///
/// The ending package must be loaded and remain loaded until its task
/// and panels finish. Requires the live completed-run save, gameplay catalog,
/// restored menu display configuration and initialized active task list.
/// Only one award sequence may use the package's shared totals at a time.
/// Spawns a bodyless controller with both payload words zero; its callback
/// later loads the award UI, presents bonuses and offers saving before restart.
/// Returns the new task, or NULL if allocation fails; it does not wait.
Task* endingSpawnReplayAwardScreen(void);

#endif // GAMEPLAY_ENDING_H
